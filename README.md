# PV198

Your personal repository for **PV198 One-chip Controllers** homeworks.
Only you and your tutor can see it. All homework submissions go here - there is no other upload channel.

- Keep each project in a separate folder.
- Do not upload the MCUXpresso workspace to git.

## Homework

Most weeks, you will receive a homework assignment. Each submission must:

- follow the required [format](#format).
- be submitted **by 24:00 on the day before your next seminar**.
- be functionally correct as specified in the assignment.
- follow good embedded software development practices.
- be implemented by you. **Using LLMs or code from other students is strictly prohibited.**

### Feedback

For each homework submission, you will receive feedback from your tutor consisting of two parts:

- **Points:**
  - **1 point** - The submission correctly implements the assignment and follows good development practices.
  - **0.5 points** - The submission has a minor functional defect, does not fully follow good development practices, or both.
  - **0 points** - The submission does not meet the functional requirements or is implemented very poorly.
- **Notes** about the submission, including instructions on what to fix where applicable.

#### Corrections

If you receive **0.5 or 0 points**, you may submit a corrected version before the deadline for the next homework assignment.

Under normal circumstances, a corrected submission can improve your score by at most **0.5 points**. Exceptions are at the discretion of your tutor.

### Format

Put your entire MCUXpresso project into this repository, including the complete project folder-not just the source files.

If the repository contains multiple projects, it must be **clear which project corresponds to the current week's assignment**. Use an unambiguous project or directory name so that your tutor does not have to guess which project should be evaluated.

Commit and tag your submission:

```sh
git add .
git commit -m "Implement hw01"
git tag Submission_01_1 # `Submission_xx_y` - `xx` = week number (leading zero), `y` = attempt number.
git push --follow-tags
```

**Note:** Only the latest tag is checked.

## Links

- Study materials: https://pv198.pages.fi.muni.cz/
- Discord (use your xlogin as your nickname): https://discord.gg/UUyNayBPkk