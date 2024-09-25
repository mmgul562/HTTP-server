# To-Do HTTP Server in C

This project is a multithreaded To-Do HTTP server implemented in C, containerized with Docker, and using PostgreSQL as the database backend. It provides an authentication system and CRUD operations for managing to-do items.

## Requirements

- Docker
- A valid SMTP server for sending emails (for account verification and password resets)

## Getting Started

1. Clone the repository
2. Rename `.env.example` file in the project root to `.env` and replace the values
3. Build and run the Docker containers:
   ```shell
   docker-compose up --build
   ```
4. The server should now be running and accessible at `http://localhost:8080`

## Features

- User Authentication
  - Authenticate with email and password
  - Email verification for new accounts
- To-Do Management
  - Create new to-do items
  - Read existing to-do items
  - Update to-do items
  - Delete to-do items
- User Account Management
  - Update email address (with verification)
  - Update password
  - Delete account
- Password Reset
  - "Forgot password" functionality
  - Password reset via email link

---
#### Note: For email functionality, the app uses the SMTP server credentials provided in the `.env` file. In production environments, you'd probably want to use a dedicated mail API service.

## Available Routes

### General
- `GET /` - Home page
- `GET /about` - About page

### User Authentication and Management
- `GET /user` - Get user management page
- `GET /user/auth` - Get user authentication page
- `POST /user/signup` - Register a new user
- `POST /user/login` - Authenticate a user
- `POST /user/logout` - Log out a user
- `POST /user/verify` - Verify user's email
- `GET /user/verify` - Get email verification page (expects verification token as query parameter)
- `POST /user/verify-new` - Verify user's new email
- `POST /user/forgot-password` - Initiate password reset process
- `GET /user/reset-password` - Get password reset page (expects verification token as query parameter)
- `POST /user/reset-password` - Reset user's password
- `PATCH /user` - Update user information
- `DELETE /user` - Delete user's account

### To-Do Management
- `POST /todo` - Create a new to-do
- `PATCH /todo/<id>` - Update a specific to-do
- `DELETE /todo/<id>` - Delete a specific to-do

---
#### Note 1: This project is not a REST API; the routes are generally designed to be accessed via the app's simple frontend. Using tools like `curl` to manually send requests is only really necessary when you don't want to send actual emails, but want to verify an account.
#### Note 2: In `/src/http/routing/handlers.c` value `SEND_EMAILS` is by default set to `false`, which means no emails will be sent. If you want to keep it this way, you'll have to verify your email by manually sending a POST request to `/user/verify` with the email and verification token (accessible in the database) in the request body.

## License Information

This project uses the following third-party libraries:

- [libpq (PostgreSQL License)](https://www.postgresql.org/about/licence/)
- [libargon2 (Simplified BSD License)](https://github.com/P-H-C/phc-winner-argon2/blob/master/LICENSE) 
- [libcurl (MIT License)](https://curl.se/docs/copyright.html) 
- [OpenSSL (OpenSSL License and SSLeay License)](https://www.openssl.org/source/license.html) 

### OpenSSL License

This project includes software developed by the OpenSSL Project for use in the OpenSSL Toolkit (http://www.openssl.org/).

This product also includes cryptographic software written by Eric Young (eay@cryptsoft.com).

For the full text of the OpenSSL and SSLeay licenses, please refer to [OpenSSL License](https://www.openssl.org/source/license.html) and [Original SSLeay License](https://www.openssl.org/source/license.html).