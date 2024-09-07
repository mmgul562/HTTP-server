document.addEventListener('DOMContentLoaded', () => {
    const showPopup = (popupId) => {
        document.getElementById(popupId).classList.remove('hidden');
    };

    const hidePopup = (popupId) => {
        document.getElementById(popupId).classList.add('hidden');
    };

    const handleResponse = (response, successMessage, failureMessage) => {
        const popupId = response.ok ? 'success-popup' : 'failure-popup';
        const message = response.ok ? successMessage : failureMessage;

        document.getElementById(popupId).innerHTML = `<p>${message}</p>`;
        showPopup(popupId);
        if (response.ok) {
            setTimeout(() => {
                hidePopup(popupId);
                location.reload();
            }, 1200);
        } else {
            setTimeout(() => {
                hidePopup(popupId)
            }, 1200);
        }
    };

    const handleError = (error) => console.error('Error:', error);

    const setupAuthForm = (btnId, formTitle, postUrl) => {
        document.getElementById(btnId).addEventListener('click', () => {
            showPopup('popup-form');
            document.getElementById('form-title').textContent = formTitle;
            document.getElementById('auth-form').reset();

            const form = document.getElementById('auth-form');
            form.onsubmit = (event) => {
                event.preventDefault();

                const formData = new URLSearchParams();
                formData.append('email', document.getElementById('email').value);
                formData.append('password', document.getElementById('password').value);

                fetch(postUrl, {
                    method: 'POST',
                    headers: {'Content-Type': 'application/x-www-form-urlencoded'},
                    body: formData.toString()
                })
                    .then(response => {
                        hidePopup('popup-form');
                        handleResponse(response, `${formTitle} successful!`, `${formTitle} failed`)
                    })
                    .catch(handleError);
            };
        });
    };

    setupAuthForm('sign-in-btn', 'Sign In', '/user/login');
    setupAuthForm('sign-up-btn', 'Sign Up', '/user/signup');

    document.getElementById('close-btn').addEventListener('click', () => hidePopup('popup-form'));
});