CREATE TABLE IF NOT EXISTS users
(
    id                 SERIAL PRIMARY KEY,
    email              VARCHAR(128) UNIQUE   NOT NULL,
    password           VARCHAR(128)          NOT NULL,
    is_verified        BOOLEAN DEFAULT FALSE NOT NULL,
    verification_token CHAR(64) UNIQUE,
    token_expires_at   TIMESTAMP
);

CREATE TABLE IF NOT EXISTS verification_results
(
    id         SERIAL PRIMARY KEY,
    token      CHAR(64),
    expires_at TIMESTAMP NOT NULL DEFAULT NOW() + INTERVAL '2 minutes',
    message    VARCHAR(256),
    success    BOOLEAN   NOT NULL
);

CREATE TABLE IF NOT EXISTS email_change_requests
(
    id                 SERIAL PRIMARY KEY,
    verification_token CHAR(64) UNIQUE NOT NULL,
    user_id            INT             NOT NULL,
    new_email          VARCHAR(128)    NOT NULL,
    token_expires_at   TIMESTAMP       NOT NULL,
    FOREIGN KEY (user_id)
        REFERENCES users (id)
        ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS sessions
(
    id         SERIAL PRIMARY KEY,
    user_id    INT             NOT NULL,
    token      CHAR(64) UNIQUE NOT NULL,
    csrf_token CHAR(64),
    expires_at TIMESTAMP       NOT NULL,
    FOREIGN KEY (user_id)
        REFERENCES users (id)
        ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS todos
(
    id            SERIAL PRIMARY KEY,
    user_id       INT           NOT NULL,
    creation_time TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    summary       VARCHAR(128)  NOT NULL,
    task          VARCHAR(2048) NOT NULL,
    due_time      TIMESTAMP,
    FOREIGN KEY (user_id)
        REFERENCES users (id)
        ON DELETE CASCADE
);
