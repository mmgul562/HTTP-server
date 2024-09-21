document.addEventListener('DOMContentLoaded', () => {
    document.getElementById('reset-password-form').onsubmit = (event) => {
        event.preventDefault();
        const urlParams = new URLSearchParams(window.location.search);

        const formData = new URLSearchParams();
        if (!checkPasswords('password', 'confirm-password')) {
            return;
        }
        formData.append('password', document.getElementById('password').value);
        formData.append('vtoken', urlParams.get('v'));

        fetch('/user/reset-password', {
            method: 'POST',
            headers: {'Content-Type': 'application/x-www-form-urlencoded'},
            body: formData.toString(),
        })
            .then(response => {
                handleResponse(
                    response,
                    'Password successfully reset!',
                    true,
                    false,
                   1200
                );
            })
            .catch(handleError);
    };
});
