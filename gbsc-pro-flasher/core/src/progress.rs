//! Progress events of a long job, shared by the CLI and the app.

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Progress {
    Status(String),
    Bytes { done: u64, total: u64 },
    Done(String),
}

pub type ProgressFn<'a> = &'a mut dyn FnMut(Progress);
