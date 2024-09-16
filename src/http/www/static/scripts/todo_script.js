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

    const csrfToken = document.querySelector('meta[name="csrf-token"]').getAttribute('content');

    document.querySelectorAll('.complete-btn').forEach((btn, index) => {
        btn.addEventListener('click', () => {
            showPopup('confirmation-popup');
            document.getElementById('confirm-btn').dataset.todoIndex = index;
        });
    });

    document.getElementById('confirm-btn').addEventListener('click', function () {
        const todoIndex = this.dataset.todoIndex;
        const todoId = document.querySelectorAll('.todo-item')[todoIndex].dataset.todoId;

        fetch(`/todo/${todoId}`, {
            method: 'DELETE',
            headers: {'X-CSRF-Token': csrfToken}
        })
            .then(response => {
                hidePopup('confirmation-popup');
                handleResponse(response, 'To-Do completed!');
            })
            .catch(handleError);
    });

    document.getElementById('cancel-btn').addEventListener('click', () => {
        hidePopup('confirmation-popup');
    });

    const setupTodoForm = (btnSelector, formTitle, indexKey) => {
        document.querySelectorAll(btnSelector).forEach((btn, index) => {
            btn.addEventListener('click', () => {
                showPopup('popup-form');
                document.getElementById('form-title').textContent = formTitle;
                const form = document.getElementById('todo-form');

                if (indexKey) {
                    const todoItem = document.querySelectorAll('.todo-item')[index];
                    form.dataset.todoIndex = index;

                    document.getElementById('summary').value = todoItem.querySelector('.todo-summary').textContent;
                    document.getElementById('task').value = todoItem.querySelector('.todo-task').textContent;
                    const dueTimeElement = todoItem.querySelector('.due-time');
                    document.getElementById('due-time').value = dueTimeElement ?
                        (dueTimeElement.getAttribute('datetime') || '') : '';
                } else {
                    form.reset();
                }

                form.onsubmit = (event) => {
                    event.preventDefault();

                    const formData = new URLSearchParams();
                    formData.append('summary', document.getElementById('summary').value);
                    formData.append('task', document.getElementById('task').value);
                    const dueTime = document.getElementById('due-time').value;
                    if (dueTime) {
                        formData.append('duetime', dueTime);
                    }

                    const isEditMode = 'todoIndex' in form.dataset;
                    const todoId = isEditMode ?
                        document.querySelectorAll('.todo-item')[form.dataset.todoIndex].dataset.todoId : '';

                    fetch(isEditMode ? `/todo/${todoId}` : '/todo', {
                        method: isEditMode ? 'PATCH' : 'POST',
                        headers: {
                            'Content-Type': 'application/x-www-form-urlencoded',
                            'X-CSRF-Token': csrfToken
                        },
                        body: formData.toString()
                    })
                        .then(response => {
                            if (response.ok) hidePopup('popup-form');
                            handleResponse(
                                response,
                                isEditMode ? 'To-Do updated successfully!' : 'To-Do added successfully!'
                            );
                        })
                        .catch(handleError);
                };
            });
        });
    };

    setupTodoForm('.edit-btn', 'Update Your To-Do', true);
    setupTodoForm('#add-todo-btn', 'Add New To-Do', false);
});