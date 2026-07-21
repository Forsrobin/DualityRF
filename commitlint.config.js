// Conventional Commits, restricted to the three types this project allows.
// A trailing `!` on the type (e.g. `feat!:`) marks a breaking change and, per
// the release pipeline, triggers a major version bump.
module.exports = {
  extends: ['@commitlint/config-conventional'],
  rules: {
    // Only feat / fix / chore are permitted.
    'type-enum': [2, 'always', ['feat', 'fix', 'chore']],
    'type-empty': [2, 'never'],
    'type-case': [2, 'always', 'lower-case'],
    // A description is mandatory.
    'subject-empty': [2, 'never'],
  },
};
