CREATE TABLE IF NOT EXISTS users
(
    id              SERIAL PRIMARY KEY,
    email           VARCHAR(128) UNIQUE NOT NULL,
    hashed_password TEXT                NOT NULL
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
