CREATE TABLE IF NOT EXISTS users
(
    id              SERIAL PRIMARY KEY,
    email           VARCHAR(128) UNIQUE NOT NULL,
    hashed_password TEXT                NOT NULL
);

CREATE TABLE IF NOT EXISTS sessions
(
    id         SERIAL PRIMARY KEY,
    user_id    INT                NOT NULL,
    token      VARCHAR(64) UNIQUE NOT NULL,
    expires_at TIMESTAMP          NOT NULL,
    FOREIGN KEY (user_id)
        REFERENCES users (id)
        ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS todos
(
    id            SERIAL PRIMARY KEY,
    user_id       INT           NOT NULL,
    creation_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    summary       VARCHAR(128)  NOT NULL,
    task          VARCHAR(2048) NOT NULL,
    due_time      TIMESTAMP,
    FOREIGN KEY (user_id)
        REFERENCES users (id)
        ON DELETE CASCADE
);