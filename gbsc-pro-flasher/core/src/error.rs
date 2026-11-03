//! Flash errors shared by the ADV Controller and ESP8266 paths, with the text the user reads.
use std::fmt;

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum FlashError {
    /// the bootloader never answered
    NoBootloader,
    /// the device refused the transfer (with what it said)
    Rejected(String),
    /// the port went away or failed: cable pulled, device reset
    PortGone,
    Cancelled,
    /// the written image does not match the file
    Verify(String),
    /// the ESP8266 flasher stub did not start
    Stub(String),
    /// the image is larger than the flash area the firmware may use (bytes)
    TooLarge { size: usize, max: usize },
}

impl FlashError {
    pub fn user_text(&self) -> String {
        match self {
            FlashError::NoBootloader => "The device did not answer. Put it in bootloader mode (ADV Controller: hold the \
                button while plugging the USB-C cable) and try again."
                .into(),
            FlashError::Rejected(why) => format!("The device stopped the transfer ({why}). Unplug it, plug it back \
                in (the ADV Controller: hold its button while plugging the USB-C cable) and try again."),
            FlashError::PortGone => "The connection was lost. Check the USB cable and flash again (the ADV \
                Controller: hold its button while plugging the USB-C cable back in). Nothing is lost: both chips keep \
                their bootloader."
                .into(),
            FlashError::Cancelled => "Flashing was cancelled.".into(),
            FlashError::Verify(what) => format!("The check after writing failed ({what}). Flash again; if it fails \
                twice, try another USB cable or port."),
            FlashError::Stub(what) => format!("The fast flasher did not start on the ESP8266 ({what}). Unplug and \
                replug the GBSC and try again."),
            FlashError::TooLarge { size, max } => format!("The image is too large for the GBSC: {size} bytes, at most \
                {max} fit before the WiFi and radio calibration area. Nothing was written: check that this is a \
                GBS-Control firmware."),
        }
    }
}

impl fmt::Display for FlashError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(&self.user_text())
    }
}

impl std::error::Error for FlashError {}

impl From<std::io::Error> for FlashError {
    fn from(_: std::io::Error) -> Self {
        FlashError::PortGone
    }
}
