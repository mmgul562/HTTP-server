const showPopup = (popupId) => {
    document.getElementById(popupId).classList.remove('hidden');
};

const hidePopup = (popupId) => {
    document.getElementById(popupId).classList.add('hidden');
};

const handleResponse = (response, successMessage, failureMessage, redirect = false) => {
    const isResponseOk = response.ok || (redirect && response.redirected);
    const popupId = isResponseOk ? 'success-popup' : 'failure-popup';
    const message = isResponseOk ? successMessage : failureMessage;

    document.getElementById(popupId).innerHTML = `<p>${message}</p>`;
    showPopup(popupId);

    if (isResponseOk && redirect) {
        setTimeout(() => {
            hidePopup(popupId);
            location.replace(response.url);
        }, 1200);
    } else {
        setTimeout(() => {
            hidePopup(popupId);
            if (isResponseOk) {
                location.reload();
            }
        }, 1200);
    }
};

const handleError = (error) => console.error('Error:', error);

const setupForm = (btnId, formId, formTitle, submitHandler) => {
    document.getElementById(btnId).addEventListener('click', () => {
        showPopup('popup-form');
        document.getElementById('form-title').textContent = formTitle;
        const form = document.getElementById(formId);
        form.reset();
        form.onsubmit = submitHandler;
    });
};

document.addEventListener('DOMContentLoaded', () => {
    document.getElementById('close-btn').addEventListener('click', () => hidePopup('popup-form'));
});