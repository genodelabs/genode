#[cfg(target_env = "armv7")]
macro_rules! dmb {
    () => {{
        asm!("dmb");
    }}
}

#[cfg(target_env = "armv6")]
macro_rules! dmb {
    () => {{
        asm!("MCR p15,0,r0,c7,c10,4");
    }}
}

macro_rules! sync_val_compare_and_swap {
    ($ptr:expr, $oldval:expr, $newval:expr) => {{
        unsafe {
        dmb!();
        let tmp = *$ptr;
        dmb!();
        if tmp == $oldval {
            dmb!();
            *$ptr = $newval;
            dmb!();
        }
        tmp
        }
    }}
}

#[no_mangle]
pub extern fn __sync_val_compare_and_swap_1(ptr: *mut u8,oldval: u8,newval: u8) -> u8 {
    sync_val_compare_and_swap!(ptr,oldval,newval)
}
#[no_mangle]
pub extern fn __sync_val_compare_and_swap_2(ptr: *mut u16,oldval: u16,newval: u16) -> u16 {
    sync_val_compare_and_swap!(ptr,oldval,newval)
}
#[no_mangle]
pub extern fn __sync_val_compare_and_swap_4(ptr: *mut u32,oldval: u32,newval: u32) -> u32 {
    sync_val_compare_and_swap!(ptr,oldval,newval)
}
#[no_mangle]
pub extern fn __sync_val_compare_and_swap_8(ptr: *mut u64,oldval: u64,newval: u64) -> u64 {
    sync_val_compare_and_swap!(ptr,oldval,newval)
}
