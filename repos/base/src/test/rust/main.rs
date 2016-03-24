#![no_std]
#![feature(lang_items,collections)]
extern crate collections;
extern crate libc;
extern "C"{
    fn print_num(num: libc::c_int);
}
#[no_mangle]
pub fn main() -> libc::c_int{
    unsafe {
        print_num(42);
    }
    0
}

#[lang="panic_fmt"]
#[no_mangle]
pub fn panic_fmt() -> ! { loop{} }

#[lang="eh_personality"]
#[no_mangle]
pub fn eh_personality() -> ! { loop{} }
