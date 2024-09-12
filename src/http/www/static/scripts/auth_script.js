document.addEventListener('DOMContentLoaded', () => {
    const handleAuth = (url, isSignIn) => (event) => {
        event.preventDefault();
        const formData = new URLSearchParams();
        formData.append('email', document.getElementById('email').value);
        formData.append('password', document.getElementById('password').value);

        fetch(url, {
            method: 'POST',
            headers: {'Content-Type': 'application/x-www-form-urlencoded'},
            body: formData.toString(),
        })
        .then(response => {
            if (response.ok) hidePopup('popup-form');
            handleResponse(
                response,
                isSignIn ? 'Successfully signed in!' : 'Successfully signed up!',
                isSignIn
            );
        })
        .catch(handleError);
    };

    setupForm('sign-in-btn', 'auth-form', 'Sign In', handleAuth('/user/login', true));
    setupForm('sign-up-btn', 'auth-form', 'Sign Up', handleAuth('/user/signup', false));
});