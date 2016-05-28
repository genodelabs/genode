#![no_std]
#![feature(asm,cfg_target_feature)]

#[cfg(target_arch = "arm")]
pub mod atomics;
