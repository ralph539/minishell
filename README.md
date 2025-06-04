# Minishell
A minimalist Unix shell implemented in C. Developed by Ralph Khairallah as part of the Operating Systems course.

## 🌐 Languages
- 🇫🇷 French version: `minishell_fr.c` + `reports/Rapport.pdf`
- 🇬🇧 English version: `minishell_eng.c` + `reports/Report.pdf`

## 📁 Structure

\`\`\`
.
├── src/           # Source code
├── reports/       # PDF reports
├── Makefile       # Build instructions
\`\`\`

## 🧪 Build & Run

\`\`\`bash
make               # Builds both minishell_eng and minishell_fr
./minishell_eng    # Run English version
./minishell_fr     # Run French version
\`\`\`

## 🔧 Features
- Process control: `fork`, `exec`, `wait`
- Signal handling: `SIGINT`, `SIGTSTP`, `SIGCHLD`
- Redirections: `<`, `>`
- Built-in commands: `cd`, `dir`
- Pipeline support: `|`
- Background execution: `&`
EOF

git add README.md
git commit -m "Add project README"
git push

