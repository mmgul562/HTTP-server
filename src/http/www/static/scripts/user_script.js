document.addEventListener('DOMContentLoaded', () => {
    const setupUserUpdateForm = (btnId, formTitle, isEmail) => {
        setupForm(btnId, 'user-update-form', formTitle, (event) => {
            event.preventDefault();

            const formData = new URLSearchParams();
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
                );
            })
            .catch(handleError);
        });

        if (isEmail) {
            document.querySelector('.email-form-container').classList.remove('hidden');
            document.querySelector('.password-form-container').classList.add('hidden');
            document.getElementById('password').required = false;
        } else {
            document.querySelector('.password-form-container').classList.remove('hidden');
            document.querySelector('.email-form-container').classList.add('hidden');
            document.getElementById('email').required = false;
        }
    };

    setupUserUpdateForm('change-email-btn', 'Change E-Mail', true);
    setupUserUpdateForm('change-password-btn', 'Change Password', false);

    document.getElementById('sign-out-btn').addEventListener('click', () => {
        fetch('/user/logout', {
            method: 'POST',
            headers: {'Cookie': document.cookie},
        })
        .then(response => handleResponse(response,
            'Successfully signed out!',
            'Failed to sign out'))
        .catch(handleError);
    });

    document.getElementById('delete-account-btn').addEventListener('click', () => {
        showPopup('confirmation-popup');
    });

    document.getElementById('confirm-btn').addEventListener('click', () => {
        fetch('/user', {
            method: 'DELETE'
        })
        .then(response => {
            hidePopup('confirmation-popup');
            handleResponse(response,
                'Account deleted successfully!',
                'Failed to delete the account');
        })
        .catch(handleError);
    });

    document.getElementById('cancel-btn').addEventListener('click', () => {
        hidePopup('confirmation-popup');
    });
});
