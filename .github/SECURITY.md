# Security Policy

## Reporting a Vulnerability

Relic Core handles secrets (passwords, private keys, seed phrases), so
security reports are taken seriously.

If you believe you have found a security vulnerability, please **do not open a
public issue**. Instead, report it privately using GitHub's
**Private vulnerability reporting**:

1. Go to the **Security** tab of the repository.
2. Click **Report a vulnerability**.
3. Describe the issue, the affected version, and (if possible) how to
   reproduce it.

You can expect:

- An acknowledgement within a few days.
- A fix coordinated with the disclosure of the vulnerability.

## Supported Versions

| Version | Supported          |
|---------|--------------------|
| latest  | :white_check_mark: |
| < 1.5.0 | :x:                |

## What to include in a report

- A clear description of the vulnerability.
- The firmware version (or commit SHA) affected.
- Steps to reproduce, if available.
- The potential impact (e.g. secret disclosure, memory corruption).
