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


CREATE OR REPLACE FUNCTION cleanup_verification_results()
RETURNS INTEGER AS $$
DECLARE
    deleted_count INTEGER;
BEGIN
    DELETE FROM verification_results
    WHERE expires_at < NOW();

    GET DIAGNOSTICS deleted_count = ROW_COUNT;
    RETURN deleted_count;
END;
$$ LANGUAGE plpgsql;


CREATE OR REPLACE FUNCTION cleanup_email_change_requests()
RETURNS INTEGER AS $$
DECLARE
    deleted_count INTEGER;
BEGIN
    DELETE FROM email_change_requests
    WHERE token_expires_at < NOW();

    GET DIAGNOSTICS deleted_count = ROW_COUNT;
    RETURN deleted_count;
END;
$$ LANGUAGE plpgsql;


CREATE OR REPLACE FUNCTION cleanup_sessions()
RETURNS INTEGER AS $$
DECLARE
    deleted_count INTEGER;
BEGIN
    DELETE FROM sessions
    WHERE expires_at < NOW();

    GET DIAGNOSTICS deleted_count = ROW_COUNT;
    RETURN deleted_count;
END;
$$ LANGUAGE plpgsql;


CREATE OR REPLACE FUNCTION cleanup_all()
RETURNS TABLE (table_name TEXT, deleted_count INTEGER) AS $$
DECLARE
    ver_count INTEGER;
    email_count INTEGER;
    session_count INTEGER;
BEGIN
    ver_count := cleanup_verification_results();
    email_count := cleanup_email_change_requests();
    session_count := cleanup_sessions();

    RETURN QUERY VALUES
        ('verification_results', ver_count),
        ('email_change_requests', email_count),
        ('sessions', session_count);
END;
$$ LANGUAGE plpgsql;