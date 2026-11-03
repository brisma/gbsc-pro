//! GBSC PRO flasher core: everything the CLI and the app do, behind plain functions and a serial link trait.
pub mod detect;
pub mod device;
pub mod error;
pub mod esp;
pub mod image;
pub mod link;
pub mod progress;
pub mod release;
pub mod version;
pub mod ymodem;

pub use error::FlashError;
