# To-Do HTTP server in C

This project is a To-Do HTTP server implemented in C, containerized with Docker,
and using PostgreSQL as the database backend. It provides an authentication system and CRUD operations for managing to-do items.

## Requirements

- Docker
- A valid SMTP server for sending emails (for account verification and password resets)

## Getting Started

1. Clone the repository
2. Setup environment variables:
  - rename `.env.example` file in the project root to `.env`
  - fill in the appropriate values
3. Build and run the docker containers:
   ```
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
#### Note: For email functionality, the app uses the SMTP server credentials provided in the .env file. In production environments, you'd probably want to use a dedicated mail API service.

## Available Routes

### General
- `GET /` - Home page
- `GET /about` - About page

### User Authentication and Management
- `GET /user` - Get authentication / user management page
- `POST /user/signup` - Register a new user
- `POST /user/login` - Authenticate a user
- `POST /user/logout` - Log out a user
- `POST /user/verify` - Verify user's email
- `GET /user/verify` - Get email verification page - expects verification token in query string
- `POST /user/verify-new` - Verify user's new email
- `POST /user/forgot-password` - Initiate password reset process
- `GET /user/reset-password` - Get password reset page - expects verification token in query string
- `POST /user/reset-password` - Reset user's password
- `PATCH /user` - Update user information
- `DELETE /user` - Delete user's account

### To-Do Management
- `POST /todo` - Create a new todo
- `PATCH /todo/<id>` - Update a specific todo
- `DELETE /todo/<id>` - Delete a specific todo

---
#### Note 1: This project is not a REST API; the routes are designed to be accessed via the app's simple frontend.
#### Note 2: In `/src/http/routing/handlers.c` macro `SEND_EMAILS` is by default set to `false`, which means no emails will be sent. In that case, you'll have to verify your email by manually sending POST request to `/user/verify` with email and verification token (accessible in database) in the body.