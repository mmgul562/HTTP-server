document.addEventListener('DOMContentLoaded', () => {
    const showPopup = (popupId) => {
        document.getElementById(popupId).classList.remove('hidden');
    };

    const hidePopup = (popupId) => {
        document.getElementById(popupId).classList.add('hidden');
    };

    const handleResponse = (response, successMessage, failureMessage) => {
        const isResponseOk = response.ok || response.status === 303;
        const popupId = isResponseOk ? 'success-popup' : 'failure-popup';
        const message = isResponseOk ? successMessage : failureMessage;

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

    const setupUserUpdateForm = (btnId, formTitle, isEmail) => {
        document.getElementById(btnId).addEventListener('click', () => {
            showPopup('popup-form');
            document.getElementById('form-title').textContent = formTitle;
            if (isEmail) {
                document.querySelector('.email-form-container').classList.remove('hidden');
                document.querySelector('.password-form-container').classList.add('hidden');
                document.getElementById('password').required = false;
            } else {
                document.querySelector('.password-form-container').classList.remove('hidden');
                document.querySelector('.email-form-container').classList.add('hidden');
                document.getElementById('email').required = false;
            }

            const form = document.getElementById('user-update-form');
            form.reset();

            form.onsubmit = (event) => {
                event.preventDefault();

                const formData = new URLSearchParams();
                formData.append('email', document.getElementById('email').value);
                if (isEmail) {
                    formData.append('email', document.getElementById('email').value);
                } else {
                    formData.append('password', document.getElementById('password').value);
                }

                fetch('/user', {
                    method: 'PATCH',
                    headers: {'Content-Type': 'application/x-www-form-urlencoded'},
                    body: formData.toString(),
                })
                    .then(response => {
                        hidePopup('popup-form');
                        handleResponse(response,
                            isEmail ? `E-Mail updated successfully!` : `Password updated successfully!`,
                            isEmail ? `Failed to update the E-Mail` : `Failed to update the password`
                        )
                    })
                    .catch(handleError);
            };
        });
    };

    setupUserUpdateForm('change-email-btn', 'Change E-Mail', true);
    setupUserUpdateForm('change-password-btn', 'Change Password', false);

    document.getElementById('close-btn').addEventListener('click', () => hidePopup('popup-form'));

    document.getElementById('sign-out-btn').addEventListener('click', () => {
        fetch('/user/logout', {
            method: 'POST',
            headers: {'Cookie': document.cookie},
        })
            .then(response => handleResponse(response,
                    'Successfully signed out!',
                    'Failed to sign out'))
            .catch(handleError);
    })

    document.getElementById('delete-account-btn').addEventListener('click', () =>{
        showPopup('confirmation-popup');
    })

    document.getElementById('confirm-delete-btn').addEventListener('click', () => {
        fetch('/user', {
            method: 'DELETE'
        })
            .then(response => {
                hidePopup('confirmation-popup');
                handleResponse(response,
                    'Account deleted successfully!',
                    'Failed to delete account')
            })
            .catch(handleError);
    })

    document.getElementById('cancel-delete-btn').addEventListener('click', () => {
        hidePopup('confirmation-popup');
    })
});
