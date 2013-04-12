#ifndef KVM__VGA_H
#define KVM__VGA_H

struct kvm;
struct framebuffer;

struct framebuffer *vga_init(struct kvm *kvm);
int vga_exit(struct kvm *kvm);

#endif /* KVM__VGA_H */
