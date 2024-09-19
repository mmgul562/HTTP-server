document.addEventListener('DOMContentLoaded', () => {
    const setupAuthForm = (btnId, formTitle, url, isSignIn) => {
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

                if (!isSignIn) {
                    const password = document.getElementById('password').value;
                    const confirmPassword = document.getElementById('confirm-password').value;

                    if (password !== confirmPassword) {
                        document.getElementById('failure-popup').innerHTML = '<p>Passwords do not match.</p>';
                        showPopup('failure-popup');
                        setTimeout(() => {
                            hidePopup('failure-popup');
                        }, 4000);
                        return;
                    }
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