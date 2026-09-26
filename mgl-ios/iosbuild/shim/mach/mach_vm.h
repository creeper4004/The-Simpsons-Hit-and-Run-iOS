/*
 * shim/mach/mach_vm.h — iOS Simulator SDK 27 marks <mach/mach_vm.h>
 * "#error unsupported", but vm_allocate/vm_deallocate exist at runtime.
 * Provide the subset MGL uses, typed against the simulator SDK's own
 * mach headers.  This dir must precede the sysroot in -I order.
 */
#ifndef _SHIM_MACH_MACH_VM_H_
#define _SHIM_MACH_MACH_VM_H_

#include <mach/mach_types.h>
#include <mach/vm_types.h>
#include <mach/vm_attributes.h>
#include <mach/vm_behavior.h>
#include <mach/vm_inherit.h>
#include <mach/vm_prot.h>
#include <mach/vm_sync.h>
#include <mach/mach_init.h>
#include <mach/mach_traps.h>

__BEGIN_DECLS

extern kern_return_t vm_allocate(vm_map_t target_task,
                                 vm_address_t *address,
                                 vm_size_t size,
                                 int flags);
extern kern_return_t vm_deallocate(vm_map_t target_task,
                                   vm_address_t address,
                                   vm_size_t size);

__END_DECLS

#endif /* _SHIM_MACH_MACH_VM_H_ */
