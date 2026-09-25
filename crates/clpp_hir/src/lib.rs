//! Call-graph filter used before emit. A name that is defined and never called is dead.

pub fn dead_names(defined: &[&str], called: &[&str]) -> Vec<String> {
    defined
        .iter()
        .filter(|name| !called.contains(name))
        .map(|name| (*name).to_string())
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn drops_uncalled_method() {
        let dead = dead_names(&["Add", "Cleanup"], &["Add"]);
        assert_eq!(dead, ["Cleanup"]);
    }
}
