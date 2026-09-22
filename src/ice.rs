//! ICE protocol stub (deferred arena / parallel checker companion).

/// Internal compiler error payload for hosts / Cluaupp doctor.
#[derive(Debug, Clone)]
pub struct IceReport {
    pub message: String,
    pub location: Option<String>,
    pub hint: &'static str,
}

impl IceReport {
    pub fn new(message: impl Into<String>) -> Self {
        Self {
            message: message.into(),
            location: None,
            hint: "please file a bug with the source that triggered this ICE",
        }
    }

    pub fn format(&self) -> String {
        match &self.location {
            Some(loc) => format!("internal error at {loc}: {}\n{}", self.message, self.hint),
            None => format!("internal error: {}\n{}", self.message, self.hint),
        }
    }
}

/// Placeholder for future arena-backed AST nodes (RFC deferred).
pub struct ArenaHint;
