const showPopup = (popupId) => {
    document.getElementById(popupId).classList.remove('hidden');
};

const hidePopup = (popupId) => {
    document.getElementById(popupId).classList.add('hidden');
};

const checkPasswords = (passwordId1, passwordId2) => {
    const password = document.getElementById(passwordId1).value;
    const confirmPassword = document.getElementById(passwordId2).value;

    if (password !== confirmPassword) {
        document.getElementById('failure-popup').innerHTML = '<p>Passwords do not match.</p>';
        showPopup('failure-popup');
        setTimeout(() => {
            hidePopup('failure-popup');
        }, 4000);
        return false;
    }
    return true;
}

const handleResponse = (response, successMessage, redirect = false, reload = true, timeout = 1200) => {
    if (response.ok || (redirect && response.redirected)) {
        const popupId = 'success-popup';
        document.getElementById(popupId).innerHTML = `<p>${successMessage}</p>`;
        showPopup(popupId);

        if (redirect) {
            setTimeout(() => {
                hidePopup(popupId);
                location.replace(response.url);
            }, timeout);
        } else {
            setTimeout(() => {
                hidePopup(popupId);
                if (reload) {
                    location.reload();
                }
            }, timeout);
        }
    } else {
        response.json().then(data => {
            const popupId = 'failure-popup';
            const failureMessage = data.error && data.error.message ? data.error.message : 'An error occurred.';
            document.getElementById(popupId).innerHTML = `<p>${failureMessage}</p>`;
            showPopup(popupId);

            setTimeout(() => {
                hidePopup(popupId);
            }, 4000);
        });
    }
};

const handleError = (error) => console.error('Error:', error);

document.addEventListener('DOMContentLoaded', () => {
    document.getElementById('close-btn').addEventListener('click', () => hidePopup('popup-form'));
});