#include "kvm/vga.h"
#include "kvm/ioport.h"
#include "kvm/kvm.h"
#include "kvm/framebuffer.h"

/* Sequencer registers */
#define VGA_SEQ_INDEX			0x3c4
#define VGA_SEQ_DATA			0x3c5
#define VGA_SEQ_RESET			0x00
#define VGA_SEQ_CLOCKING_MODE		0x01
#define VGA_SEQ_MAP_MASK		0x02
#define VGA_SEQ_CHARACTER_MAP_SELECT	0x03
#define VGA_SEQ_MEMORY_MODE		0x04

/* CRT Controller Registers */
#define VGA_CRT_COLOR_INDEX		0x3b4
#define VGA_CRT_COLOR_DATA		0x3b5
#define VGA_CRT_MONO_INDEX		0x3d4
#define VGA_CRT_MONO_DATA		0x3d5

/* Graphics Registers */
#define VGA_GFX_INDEX			0x3ce
#define VGA_GFX_DATA			0x3cf
#define VGA_GFX_SET_RESET		0x00
#define VGA_GFX_ENABLE_SET_RESET	0x01
#define VGA_GFX_COLOR_COMPARE		0x02
#define VGA_GFX_DATA_ROTATE		0x03
#define VGA_GFX_READ_MAP_SELECT		0x04
#define VGA_GFX_GRAPHIC_MODE		0x05
#define VGA_GFX_GRAPHIC_MODE_OE		(1 << 4)
#define VGA_GFX_GRAPHIC_MODE_RM 	(1 << 3)
#define VGA_GFX_GRAPHIC_MODE_WM		(1 << 1 | 1 << 0)
#define VGA_GFX_MISC			0x06
#define VGA_GFX_MISC_MM			(1 << 3 | 1 << 2)
#define VGA_GFX_MISC_OE			(1 << 1)
#define VGA_GFX_MISC_GM			(1 << 0)
#define VGA_GFX_COLOR_DONT_CARE		0x06
#define VGA_GFX_BIT_MASK		0x07

/* Attribute Controller Registers */
#define VGA_ATT_DATA_WRITE		0x3c0
#define VGA_ATT_DATA_READ		0x3c1
#define VGA_ATT_PALETTE0		0x00
#define VGA_ATT_PALETTEF		0x0f
#define VGA_ATT_ATTRIBUTE_MODE_CONTROL	0x10
#define VGA_ATT_OVERSCAN_COLOR		0x11
#define VGA_ATT_COLOR_PLANE_ENABLE	0x12
#define VGA_ATT_PEL			0x13
#define VGA_ATT_COLOR_SELECT		0x14

/* DAC Registers */
/* VGA_DAC_STATUS is readonly*/
#define VGA_DAC_STATUS			0x3c7
#define VGA_DAC_READ_INDEX		0x3c7
#define VGA_DAC_WRITE_INDEX		0x3c8
#define VGA_DAC_DATA			0x3c9

/* Misc Registers */
#define VGA_MISC_OUTPUT_READ		0x3cc
#define VGA_MISC_OUTPUT_WRITE		0x3c2

#define VGA_MISC_FEATURE_READ		0x3ca
#define VGA_MISC_FEATURE_WRITE_MONO	0x3ba
#define VGA_MISC_FEATURE_WRITE_COLOR	0x3da

#define VGA_MISC_INPUT_STATUS0		0x3c2 /* Readonly */
#define VGA_MISC_INPUT_STATUS1_MONO	0x3ba /* Readonly */
#define VGA_MISC_INPUT_STATUS1_COLOR	0x3da /* Readonly*/

#define VGA_MISC_ENABLE			0x3c3

/* Frame Buffer */
//#define VGA_WIDTH	320
//#define VGA_HEIGHT	200
#define VGA_WIDTH	640
#define VGA_HEIGHT	480
#define VGA_MEM_ADDR	0xd0000000
#define VGA_MEM_SIZE	(4 * VGA_WIDTH * VGA_HEIGHT)
#define VGA_BPP	32

/* 64KB for each plane */
#define PLANE_SIZE 	(64 * 1024 * 1024)

struct vga_device {
	/* CRT registers */
	u8 crt_reg_index;
	u8 crt_reg[256];
	/* Sequencer registers*/
	u8 seq_reg_index;
	u8 seq_reg[256];
	/* Graphics Registers */
	u8 gfx_reg_index;
	u8 gfx_reg[256];
	/* Attribute Registers */
	u8 att_reg_flip;
	u8 att_reg_index;
	u8 att_reg[256];
	/* DAC registers*/
	u8 dac_read_index;
	u8 dac_write_index;
	u8 dac_state;
	u8 dac_sub_reg[3];
	u8 dac_sub_index;
	u8 dac_reg[256];
	u8 dac_palette[256 * 3];
	/* Misc registers */
	u8 misc_status0;
	u8 misc_status1;
	u8 misc_feature;
	u8 misc_output;

	/* Display Memory */
	struct framebuffer fb;
	u64 vram_start;
	u64 vram_len;
	u8 *plane;

};

static struct vga_device vga = {
	.vram_start	= 0xA0000,
	.vram_len	= 0x20000,
};

#if 0
static void dump_mem(int n)
{
	int i;

	for (i = 0; i < n; i++) {
		pr_info("vga.vram[%x]=%x", i + (int)vga.vram_start, vga.vram[i]);
	}
}

static void compare_mem(int n)
{
	int i;
	int cnt=0;

	n = vga.vram_len;
	for (i = 0; i < n; i++) {
		if (vga.vram[i] != 0x55 && cnt < 0x1000)  {
			cnt++;
			pr_info("vga.vram[%x]=%x", i + (int)vga.vram_start, vga.vram[i] & 0xff );
		}
	}
}
#endif

static bool vga_ioport_in(struct ioport *ioport, struct kvm *kvm, u16 port, void *data, int size)
{
	u8 val;

	switch (port) {
	case VGA_CRT_COLOR_INDEX:
	case VGA_CRT_MONO_INDEX:
		val = vga.crt_reg_index;
		break;
	case VGA_CRT_COLOR_DATA:
	case VGA_CRT_MONO_DATA:
		val = vga.crt_reg[vga.crt_reg_index];
		break;
	case VGA_SEQ_INDEX:
		val = vga.seq_reg_index;
		break;
	case VGA_SEQ_DATA:
		val = vga.seq_reg[vga.seq_reg_index];
		break;
	case VGA_GFX_INDEX:
		val = vga.gfx_reg_index;
		break;
	case VGA_GFX_DATA:
		val = vga.gfx_reg[vga.gfx_reg_index];
	case VGA_ATT_DATA_WRITE:
		if (vga.att_reg_flip == 0)
			val = vga.att_reg_index;
		else
			val = 0;
		break;
	case VGA_ATT_DATA_READ:
		val = vga.att_reg[vga.att_reg_index & 0x1f];
		break;
	case VGA_DAC_READ_INDEX:
		val = vga.dac_read_index;
		break;
	case VGA_DAC_WRITE_INDEX:
		val = vga.dac_write_index;
		break;
	case VGA_DAC_DATA:
		val = vga.dac_palette[vga.dac_read_index * 3 + vga.dac_sub_index];
		vga.dac_sub_index++;
		if (vga.dac_sub_index == 3) {
			vga.dac_sub_index = 0;
			vga.dac_read_index++;
		}
		break;
	case VGA_MISC_INPUT_STATUS0:
		val = vga.misc_status0;
		break;
	case VGA_MISC_OUTPUT_READ:
		val = vga.misc_output;
	case VGA_MISC_FEATURE_READ:
		val = vga.misc_feature;
	case VGA_MISC_INPUT_STATUS1_MONO:
	case VGA_MISC_INPUT_STATUS1_COLOR:
		val = vga.misc_status1;
		vga.att_reg_flip = 0;
	default:
		val = 0;
		break;
	}
	ioport__write8(data, val);
	return true;
}

static bool vga_ioport_out(struct ioport *ioport, struct kvm *kvm, u16 port, void *data, int size)
{
	u32 val;
	u8 index;

	val = ioport__read8(data);

	switch (port) {
	case VGA_CRT_COLOR_INDEX:
	case VGA_CRT_MONO_INDEX:
		vga.crt_reg_index = val;
		break;
	case VGA_CRT_COLOR_DATA:
	case VGA_CRT_MONO_DATA:
		vga.crt_reg[vga.crt_reg_index] = val;
		break;
	case VGA_SEQ_INDEX:
		vga.seq_reg_index = val;
		break;
	case VGA_SEQ_DATA:
		vga.seq_reg[vga.seq_reg_index] = val;
		break;
	case VGA_GFX_INDEX:
		vga.gfx_reg_index = val;
		break;
	case VGA_GFX_DATA:
		vga.gfx_reg[vga.gfx_reg_index] = val;
	case VGA_ATT_DATA_WRITE:
		if (vga.att_reg_flip == 0) {
			vga.att_reg_index = val & 0x3f;
		} else {
			index = vga.att_reg_index & 0x1f;
			switch (index) {
			case VGA_ATT_PALETTE0 ... VGA_ATT_PALETTEF:
				vga.att_reg[index] = val & 0x3f;
				break;
			case VGA_ATT_ATTRIBUTE_MODE_CONTROL:
				vga.att_reg[index] = val & ~0x10;
				break;
			case VGA_ATT_OVERSCAN_COLOR:
				vga.att_reg[index] = val;
				break;
			case VGA_ATT_COLOR_PLANE_ENABLE:
				vga.att_reg[index] = val & ~0xc0;
				break;
			case VGA_ATT_PEL:
				vga.att_reg[index] = val & ~0xf0;
				break;
			case VGA_ATT_COLOR_SELECT:
				vga.att_reg[index] = val & ~0xf0;
				break;
			}
			vga.att_reg_flip ^= 1;
		}
		break;
	case VGA_DAC_READ_INDEX:
		vga.dac_read_index = val;
		vga.dac_sub_index = 0;
		vga.dac_state = 3;
		break;
	case VGA_DAC_WRITE_INDEX:
		vga.dac_write_index = val;
		vga.dac_sub_index = 0;
		vga.dac_state = 0;
		break;
	case VGA_DAC_DATA:
		vga.dac_sub_reg[vga.dac_sub_index] = val & 0x3f;
		vga.dac_sub_index++;
		if (vga.dac_sub_index == 3) {
			memcpy(&vga.dac_palette[vga.dac_write_index * 3], vga.dac_sub_reg, 3);
			vga.dac_sub_index = 0;
			vga.dac_write_index++;
		}
		break;
	case VGA_MISC_OUTPUT_WRITE:
		vga.misc_output = val & ~0x10; /* The bit 4 is not defined */
		break;
	case VGA_MISC_FEATURE_WRITE_MONO:
	case VGA_MISC_FEATURE_WRITE_COLOR:
		vga.misc_feature = val; /* FIXME: not sure about this bit */
	case VGA_MISC_ENABLE:
		pr_info("---> debug vga ---\n");
		//dump_mem(16);
		//compare_mem(16);
	default:
		break;
	}

	return true;
}

static struct ioport_operations vga_ioport_ops = {
	.io_in		= vga_ioport_in,
	.io_out		= vga_ioport_out,
};

static u16 vga_ioport[] = {
	VGA_CRT_COLOR_INDEX,
	VGA_CRT_COLOR_DATA,
	VGA_CRT_MONO_INDEX,
	VGA_CRT_MONO_DATA,
	VGA_SEQ_INDEX,
	VGA_SEQ_DATA,
	VGA_GFX_INDEX,
	VGA_GFX_DATA,
	VGA_ATT_DATA_WRITE,
	VGA_ATT_DATA_READ,
	VGA_DAC_READ_INDEX,
	VGA_DAC_WRITE_INDEX,
	VGA_DAC_DATA,
	VGA_MISC_OUTPUT_READ,
	VGA_MISC_OUTPUT_WRITE,
	VGA_MISC_FEATURE_READ,
	VGA_MISC_FEATURE_WRITE_COLOR,
	VGA_MISC_ENABLE,
};

static int vga_init_ioport(struct kvm *kvm)
{
	int r, i, port;

	for (i = 0; i < (int)ARRAY_SIZE(vga_ioport); i ++) {
		r = ioport__register(kvm, vga_ioport[i], &vga_ioport_ops, 1, NULL);
		port = i;
		if (r < 0)
			goto err;
	}
	return 0;

err:
	for (i = 0; i < port; i ++)
		ioport__unregister(kvm, vga_ioport[i]);

	return -1;
}

static int vga_exit_ioport(struct kvm *kvm)
{
	int i;

	for (i = 0; i < (int)ARRAY_SIZE(vga_ioport); i ++)
		ioport__unregister(kvm, vga_ioport[i]);

	return 0;
}
static void vga_mmio_callback_read(u64 addr, u8 *data, u32 len, void *ptr)
{

}

static void vga_mmio_callback_write(u64 addr, u8 *data, u32 len, void *ptr)
{
	struct framebuffer *fb = &vga.fb;
	unsigned int i;
	u8 *v;
	u8 map_mask = vga.seq_reg[VGA_SEQ_MAP_MASK] & 0x0f;
	bool chain4 = vga.seq_reg[VGA_SEQ_MEMORY_MODE] & 0x08;
	u8 graphic_mode = vga.gfx_reg[VGA_GFX_GRAPHIC_MODE];
	u8 read_map_select = vga.gfx_reg[VGA_GFX_READ_MAP_SELECT];
	u8 mask;
	u8 memory_map_mode = vga.gfx_reg[VGA_GFX_MISC] & VGA_GFX_MISC_MM;
	u8 plane;

	/*
	 * 0 0   A0000 128KB
	 * 0 1   A0000  64KB
	 * 1 0   B0000  32KB
	 * 1 1   B8000  32KB
	 */
	switch (memory_map_mode) {
	case 0:
		addr -= 0xA0000;
		break;
	case 1:
		addr -= 0xA0000;
		break;
	case 2:
		addr -= 0xB0000;
		break;
	case 3:
		addr -= 0xB8000;
		break;
	default:
		break;
	}

	/* Memory mode: chain 4*/
	if (chain4) {
		/*
		 * A1 A0   Map Selct
		 * 0  0    0
		 * 0  1    1
		 * 1  0    2
		 * 1  1    3
		 */
		plane = addr & 0x03;
		mask = 1 < plane;
		if (map_mask & mask) {
			v = vga.plane + addr;
			memcpy(v, data, len);
		}
	/* Memory mode: odd/even */
	} else if (graphic_mode & VGA_GFX_GRAPHIC_MODE_OE ){
		plane = (read_map_select & 0x02) | (addr & 0x01);
		mask = 1 < plane;
		if (map_mask & mask) {
			v = vga.plane + addr;
			memcpy(v, data, len);
		}
	/* Memory mode: normal */
	} else {
		u8 write_mode = graphic_mode & VGA_GFX_GRAPHIC_MODE_WM;
		switch (write_mode) {
		case 0:
			break;
		case 1:
			break;
		case 2:
			break;
		case 3:
			break;
		default:
			break;
		}

	}

#if 0
	for (i = 0; i < fb->mem_size; i += 4) {
		fb->mem[i+0] = 0xff;
		fb->mem[i+1] = 0xff;
		fb->mem[i+2] = 0x00;
		fb->mem[i+3] = 0x00;
	}
#endif
}

static void vga_mmio_callback(u64 addr, u8 *data, u32 len, u8 is_write, void *ptr)
{
	if (is_write)
		vga_mmio_callback_write(addr, data, len, ptr);
	else
		vga_mmio_callback_read(addr, data, len, ptr);
}

struct framebuffer *vga_init(struct kvm *kvm)
{
	char *mem;
	int ret;

	ret = vga_init_ioport(kvm);
	if (ret < 0)
		return NULL;

	ret = kvm__register_mmio(kvm, vga.vram_start, vga.vram_len, false, vga_mmio_callback, kvm);
	if (ret < 0)
		return NULL;

	mem = calloc(1, PLANE_SIZE);
	if (!mem)
		return NULL;
	vga.plane = (u8 *)mem;

	mem = mmap(NULL, VGA_MEM_SIZE, PROT_RW, MAP_ANON_NORESERVE, -1, 0);
	vga.fb = (struct framebuffer) {
		.width			= VGA_WIDTH,
		.height			= VGA_HEIGHT,
		.depth			= VGA_BPP,
		.mem			= mem,
		.mem_addr		= VGA_MEM_ADDR,
		.mem_size		= VGA_MEM_SIZE,
		.kvm			= kvm,
	};
	return fb__register(&vga.fb);
}

int vga_exit(struct kvm *kvm)
{
	int ret;

	ret = vga_exit_ioport(kvm);
	free(vga.plane);

	return ret;
}
