function toggleExpand(element) {
    const todoTask = element.querySelector('.todo-task');
    const isExpanded = element.classList.contains('expanded');
    todoTask.style.maxHeight = isExpanded ? 0 : `${todoTask.scrollHeight}px`;
    element.classList.toggle('expanded', !isExpanded);
}

document.addEventListener('DOMContentLoaded', () => {
    const formatTimeElement = (element) => {
        const options = {
            year: 'numeric',
            month: '2-digit',
            day: '2-digit',
            hour: '2-digit',
            minute: '2-digit',
            hour12: false
        };
        const dateTime = new Date(element.getAttribute('datetime'));
        element.textContent = dateTime.toLocaleString(undefined, options);
    };

    const creationTimeElements = document.getElementsByClassName('creation-time');
    const dueTimeElements = document.getElementsByClassName('due-time');

    if (creationTimeElements.length > 0) {
        Array.from(creationTimeElements).forEach(el => formatTimeElement(el));
        Array.from(dueTimeElements).forEach(el => formatTimeElement(el));
    }

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
            }, 2000);
        } else {
            hidePopup(popupId);
        }
    };

    const handleError = (error) => console.error('Error:', error);

    document.querySelectorAll('.complete-btn').forEach((btn, index) => {
        btn.addEventListener('click', () => {
            showPopup('confirmation-popup');
            document.getElementById('confirm-complete-btn').dataset.todoIndex = index;
        });
    });

    document.getElementById('confirm-complete-btn').addEventListener('click', function() {
        const todoIndex = this.dataset.todoIndex;
        const todoId = document.querySelectorAll('.todo-item')[todoIndex].dataset.todoId;

        fetch(`/${todoId}`, { method: 'DELETE' })
            .then(response => {
                hidePopup('confirmation-popup');
                handleResponse(response, 'To-Do completed!', 'Failed to complete a To-Do');
            })
            .catch(handleError);
    });

    document.getElementById('cancel-complete-btn').addEventListener('click', () => {
        hidePopup('confirmation-popup');
    });

    const setupFormPopup = (btnSelector, formTitle, indexKey) => {
        document.querySelectorAll(btnSelector).forEach((btn, index) => {
            btn.addEventListener('click', () => {
                showPopup('popup-form');
                document.getElementById('form-title').textContent = formTitle;
                if (indexKey) {
                    const todoItem = document.querySelectorAll('.todo-item')[index];
                    document.getElementById('todo-form').dataset.todoIndex = index;

                    document.getElementById('summary').value = todoItem.querySelector('.todo-summary').textContent;
                    document.getElementById('task').value = todoItem.querySelector('.todo-task').textContent;
                    const dueTimeElement = todoItem.querySelector('.due-time');
                    if (dueTimeElement) {
                        const dueDateTime = dueTimeElement.getAttribute('datetime');
                        if (dueDateTime) {
                            document.getElementById('due-time').value = dueDateTime;
                        } else {
                            document.getElementById('due-time').value = '';
                        }
                    } else {
                        document.getElementById('due-time').value = '';
                    }
                } else {
                    document.getElementById('todo-form').reset();
                }
            });
        });
    };

    setupFormPopup('.edit-btn', 'Update Your To-Do', true);
    setupFormPopup('#add-todo-btn', 'Add New To-Do', false);

    document.getElementById('close-btn').addEventListener('click', () => hidePopup('popup-form'));

    document.getElementById('todo-form').addEventListener('submit', function (event) {
        event.preventDefault();

        const formData = new URLSearchParams();
        formData.append('summary', document.getElementById('summary').value);
        formData.append('task', document.getElementById('task').value);
        const dueTime = document.getElementById('due-time').value;
        if (dueTime) {
            formData.append('duetime', dueTime);
        }

        const isEditMode = 'todoIndex' in this.dataset;
        const todoIndex = this.dataset.todoIndex;
        const todoId = isEditMode ? document.querySelectorAll('.todo-item')[todoIndex].dataset.todoId : '';

        fetch(isEditMode ? `/${todoId}` : '/', {
            method: isEditMode ? 'PATCH' : 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body: formData.toString()
        })
        .then(response => {
            hidePopup('popup-form');
            handleResponse(
                response,
                isEditMode ? 'To-Do updated successfully!' : 'To-Do added successfully!',
                isEditMode ? 'Failed to update a To-Do' : 'Failed to add a To-Do'
            );
        })
        .catch(handleError);
    });
});