# Security Policy

## Supported Versions

We provide security updates, patches, and fixes for the following versions:

| Version | Supported |
| ------- | --------- |
| `main`  | Yes       |
| < 1.0   | No        |

---

## Reporting a Vulnerability

We take the security of this project seriously. Because printer drivers interact closely with system print spoolers (CUPS) and execute with specific system permissions, memory safety and buffer boundary verification are critical.

If you discover a security vulnerability:

1. **Do not open a public issue.**
2. Please report the issue privately via:
   - **GitHub Security Advisories**: [Report a vulnerability](https://github.com/ankitsingh99/ricoh-universal-ddst-driver/security/advisories/new)
   - **Email**: Send details directly to `Mail.ankitks@gmail.com` with the subject `[SECURITY] Ricoh DDST Driver Vulnerability`.

### What to Include in Your Report

To help us investigate and triage the issue quickly, please include:
- **Type of issue**: (e.g., buffer overflow, privilege escalation, unauthorized file access, memory corruption).
- **Environment**: OS name/version, CUPS version, architecture (`uname -m`).
- **Steps to reproduce**: A detailed description or Proof of Concept (PoC) file demonstrating the issue.
- **Impact**: Potential consequences of exploiting the vulnerability.

---

## Response & Disclosure Process

1. **Acknowledgement**: We will acknowledge receipt of your vulnerability report within **48 hours**.
2. **Assessment**: We will confirm the vulnerability, assess its severity, and determine an appropriate fix.
3. **Fix & Advisory**: We will develop and test a patch, release a fix to the `main` branch, and publish a security advisory crediting the reporter (unless you prefer to remain anonymous).
