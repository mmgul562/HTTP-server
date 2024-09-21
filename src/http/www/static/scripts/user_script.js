document.addEventListener('DOMContentLoaded', () => {
    const csrfToken = document.querySelector('meta[name="csrf-token"]').getAttribute('content');

    const setupUserUpdateForm = (btnId, formTitle, isEmail) => {
        document.getElementById(btnId).addEventListener('click', () => {
            showPopup('popup-form');
            document.getElementById('form-title').textContent = formTitle;
            const form = document.getElementById('user-update-form');
            form.reset();

            form.onsubmit = (event) => {
                event.preventDefault();

                const formData = new URLSearchParams();
                if (isEmail) {
                    formData.append('email', document.getElementById('email').value);
                } else {
                    if (!checkPasswords('password', 'confirm-password')) {
                        return;
                    }
                    formData.append('password', document.getElementById('password').value);
                }

                fetch('/user', {
                    method: 'PATCH',
                    headers: {
                        'Content-Type': 'application/x-www-form-urlencoded',
                        'X-CSRF-Token': csrfToken
                    },
                    body: formData.toString(),
                })
                    .then(response => {
                        if (response.ok) hidePopup('popup-form');
                        handleResponse(
                            response,
                            isEmail ? `Verification e-mail was sent to provided address.` : `Password updated successfully!`,
                            false, false,
                            isEmail ? 4000 : 1200
                        );
                    })
                    .catch(handleError);
            };
        });
    };

    setupUserUpdateForm('change-email-btn', 'Change E-Mail', true);
    document.getElementById('change-email-btn').addEventListener('click', () => {
        document.querySelector('.email-form-container').classList.remove('hidden');
        document.querySelector('.password-form-container').classList.add('hidden');
        document.getElementById('password').required = false;
    });
    setupUserUpdateForm('change-password-btn', 'Change Password', false);
    document.getElementById('change-password-btn').addEventListener('click', () => {
        document.querySelector('.password-form-container').classList.remove('hidden');
        document.querySelector('.email-form-container').classList.add('hidden');
        document.getElementById('email').required = false;
    })

    document.getElementById('sign-out-btn').addEventListener('click', () => {
        fetch('/user/logout', {
            method: 'POST',
            headers: {
                'Cookie': document.cookie,
                'X-CSRF-Token': csrfToken
            },
        })
            .then(response => handleResponse(response, 'Successfully signed out!'))
            .catch(handleError);
    });

    document.getElementById('delete-account-btn').addEventListener('click', () => {
        showPopup('confirmation-popup');
    });

    document.getElementById('confirm-btn').addEventListener('click', () => {
        fetch('/user', {
            method: 'DELETE',
            headers: {'X-CSRF-Token': csrfToken}
        })
            .then(response => {
                if (response.ok) hidePopup('confirmation-popup');
                handleResponse(response, 'Account deleted successfully!');
            })
            .catch(handleError);
    });

    document.getElementById('cancel-btn').addEventListener('click', () => {
        hidePopup('confirmation-popup');
    });
});
