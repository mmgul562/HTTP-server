document.addEventListener('DOMContentLoaded', () => {
    const showPopup = (popupId) => {
        document.getElementById(popupId).classList.remove('hidden');
    };

    const hidePopup = (popupId) => {
        document.getElementById(popupId).classList.add('hidden');
    };

    const handleSignIn = (response) => {
        const isResponseOk = response.ok && response.redirected;
        const popupId = isResponseOk ? 'success-popup' : 'failure-popup';
        const message = isResponseOk ? 'Successfully signed in!' : 'Failed to sign in';
        document.getElementById(popupId).innerHTML = `<p>${message}</p>`;
        showPopup(popupId);

        if (isResponseOk) {
            setTimeout(() => {
                hidePopup(popupId);
                location.replace(response.url);
            }, 1200)
        } else {
            setTimeout(() => {
                hidePopup(popupId);
            }, 1200)
        }
    }

    const handleSignUp = (response) => {
        const popupId = response.ok ? 'success-popup' : 'failure-popup';
        const message = response.ok ? 'Successfully signed in!' : 'Failed to sign in';
        document.getElementById(popupId).innerHTML = `<p>${message}</p>`;
        showPopup(popupId);

        setTimeout(() => {
            hidePopup(popupId);
        }, 1200);
    }

    const handleError = (error) => console.error('Error:', error);

    const setupAuthForm = (btnId, formTitle, postUrl, signIn) => {
        document.getElementById(btnId).addEventListener('click', () => {
            showPopup('popup-form');
            document.getElementById('form-title').textContent = formTitle;

            const form = document.getElementById('auth-form');
            form.reset();

            form.onsubmit = (event) => {
                event.preventDefault();

                const formData = new URLSearchParams();
                formData.append('email', document.getElementById('email').value);
                formData.append('password', document.getElementById('password').value);

                fetch(postUrl, {
                    method: 'POST',
                    headers: {'Content-Type': 'application/x-www-form-urlencoded'},
                    body: formData.toString(),
                })
                    .then(response => {
                        if (response.ok) hidePopup('popup-form');
                        signIn ? handleSignIn(response) : handleSignUp(response);
                    })
                    .catch(handleError);
            };
        });
    };

    setupAuthForm('sign-in-btn', 'Sign In', '/user/login', true);
    setupAuthForm('sign-up-btn', 'Sign Up', '/user/signup', false);

    document.getElementById('close-btn').addEventListener('click', () => hidePopup('popup-form'));
});