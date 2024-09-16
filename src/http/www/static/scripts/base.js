const showPopup = (popupId) => {
    document.getElementById(popupId).classList.remove('hidden');
};

const hidePopup = (popupId) => {
    document.getElementById(popupId).classList.add('hidden');
};

const handleResponse = (response, successMessage, redirect = false) => {
    if (response.ok || (redirect && response.redirected)) {
        const popupId = 'success-popup';
        document.getElementById(popupId).innerHTML = `<p>${successMessage}</p>`;
        showPopup(popupId);

        if (redirect) {
            setTimeout(() => {
                hidePopup(popupId);
                location.replace(response.url);
            }, 1200);
        } else {
            setTimeout(() => {
                hidePopup(popupId);
                location.reload();
            }, 1200);
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