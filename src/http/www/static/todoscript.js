function toggleExpand(element) {
    const todoTask = element.querySelector('.todo-task');
    if (element.classList.contains('expanded')) {
        todoTask.style.maxHeight = 0;
        element.classList.remove('expanded');
    } else {
        todoTask.style.maxHeight = todoTask.scrollHeight + 'px';
        element.classList.add('expanded');
    }
}

document.addEventListener('DOMContentLoaded', () => {
    const creationTimeElements = document.getElementsByClassName('creation-time');
    if (creationTimeElements.length === 0) {
        return;
    }
    const dueTimeElements = document.getElementsByClassName('due-time');
    const options = {
        year: 'numeric',
        month: '2-digit',
        day: '2-digit',
        hour: '2-digit',
        minute: '2-digit',
        hour12: false
    };
    for (const time of creationTimeElements) {
        const creationTime = new Date(time.getAttribute('datetime'));
        time.textContent = creationTime.toLocaleString(undefined, options);
    }
    for (const time of dueTimeElements) {
        const dueTime = new Date(time.getAttribute('datetime'));
        time.textContent = dueTime.toLocaleString(undefined, options);
    }
});

document.getElementById('add-todo-btn').addEventListener('click', () => {
    document.getElementById('popup-form').classList.remove('hidden');
});

document.querySelector('.close-btn').addEventListener('click', () => {
    document.getElementById('popup-form').classList.add('hidden');
});

document.getElementById('todo-form').addEventListener('submit', function(event) {
    event.preventDefault();
    const summary = document.getElementById('summary').value;
    const task = document.getElementById('task').value;
    const dueTime = document.getElementById('due-time').value;

    const formData = new URLSearchParams();
    formData.append('summary', summary);
    formData.append('task', task);
    if (dueTime) {
        formData.append('duetime', dueTime);
    }

    fetch('/', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/x-www-form-urlencoded'
        },
        body: formData.toString()
    })
    .then(response => {
        if (response.ok) {
            document.getElementById('popup-form').classList.add('hidden');
            document.getElementById('todo-form').reset();

            const successPopup = document.getElementById('success-popup');
            successPopup.classList.remove('hidden');

            setTimeout(() => {
                successPopup.classList.add('hidden');
                location.reload();
            }, 2000);
        } else {
            alert('Error adding to-do.');
        }
    })
    .catch(error => console.error('Error:', error));
});