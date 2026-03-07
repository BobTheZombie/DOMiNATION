# Strategic Deterrence System

This layer adds deterministic late-game deterrence gameplay: stockpile/readiness, posture, warning, interception, retaliation, and second-strike state.

- Data model: `strategicDeterrence` per player.
- Launch flow: unavailable -> preparing -> ready -> launched -> intercepted/detonated -> resolved.
- Defense model: warning + anti-missile coverage + electronics/doctrine modifiers produce deterministic interception outcomes.
- AI: posture and production behavior reacts to threat ratio, war/tension state, doctrine, and survivability.
