#![no_std]
#![feature(lang_items,collections,alloc)]
extern crate collections;
extern crate alloc;
extern crate libc;
use alloc::boxed::Box;
extern "C"{
    fn print_num(num: libc::c_int);
}
#[no_mangle]
pub fn main() -> libc::c_int{
    let n = Box::new(42);
    unsafe {
        print_num(*n);
    }
    0
}
#[lang="panic_fmt"]
#[no_mangle]
pub fn panic_fmt() -> ! { loop{} }

#[lang="eh_personality"]
#[no_mangle]
pub fn eh_personality() -> ! { loop{} }
