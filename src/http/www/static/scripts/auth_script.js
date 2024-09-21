document.addEventListener('DOMContentLoaded', () => {
    document.getElementById('forgot-password-btn').addEventListener('click', () => {
        hidePopup('popup-form');
        showPopup('forgot-password-popup-form');
        const form = document.getElementById('forgot-password-form');
        form.reset();

        form.onsubmit = (event) => {
            event.preventDefault();
            const formData = new URLSearchParams();
            formData.append('email', document.getElementById('forgot-password-email').value);

            fetch('/user/forgot-password', {
                method: 'POST',
                headers: {'Content-Type': 'application/x-www-form-urlencoded'},
                body: formData.toString(),
            })
                .then(response => {
                    if (response.ok) hidePopup('forgot-password-popup-form');
                    handleResponse(
                        response,
                        'Password-resetting e-mail was sent to your inbox!',
                        false,
                        false,
                        4000
                    );
                })
                .catch(handleError);
        };
    });

    document.getElementById('close-reset-btn').addEventListener('click', () => {
        hidePopup('forgot-password-popup-form');
    });

    const setupAuthForm = (btnId, formTitle, url, isSignIn) => {
        document.getElementById(btnId).addEventListener('click', () => {
            showPopup('popup-form');
            document.getElementById('form-title').textContent = formTitle;
            if (isSignIn) {
                document.getElementById('forgot-password-p').classList.remove('hidden');
            } else {
                document.getElementById('forgot-password-p').classList.add('hidden');
            }
            const form = document.getElementById('auth-form');
            form.reset();

            form.onsubmit = (event) => {
                event.preventDefault();
                const formData = new URLSearchParams();
                formData.append('email', document.getElementById('email').value);
                formData.append('password', document.getElementById('password').value);

                if (!isSignIn && !checkPasswords('password', 'confirm-password')) {
                    return;
                }

                fetch(url, {
                    method: 'POST',
                    headers: {'Content-Type': 'application/x-www-form-urlencoded'},
                    body: formData.toString(),
                })
                    .then(response => {
                        if (response.ok) hidePopup('popup-form');
                        handleResponse(
                            response,
                            isSignIn ? 'Successfully signed in!' : 'Successfully signed up! Check your inbox and verify your e-mail.',
                            isSignIn,
                            false,
                            isSignIn ? 1200 : 4000
                        );
                    })
                    .catch(handleError);
            };
        });
    };

    setupAuthForm('sign-in-btn', 'Sign In', '/user/login', true);
    document.getElementById('sign-in-btn').addEventListener('click', () => {
        document.getElementById('confirm-password-container').classList.add('hidden');
        document.getElementById('confirm-password').removeAttribute('required');
    });
    setupAuthForm('sign-up-btn', 'Sign Up', '/user/signup', false);
    document.getElementById('sign-up-btn').addEventListener('click', () => {
        document.getElementById('confirm-password-container').classList.remove('hidden');
        document.getElementById('confirm-password').setAttribute('required', 'required');
    });
});