let isDragging = false;
let currentX = 0, currentY = 0;
let speed = 0, turn = 0;
let prevSpeed = 0, prevTurn = 0;
let triggerSend = false;

const maxSpeed = parseInt(prompt("Enter max speed percentage:", "30") || 30); // Max speed percentage
const maxTurn = 30; // Max turn percentage
const controlIntervalTime = 100; // Control update interval in ms

const joystick = document.getElementById('joystick');
const container = document.getElementById('joystickContainer');
const speedDisplay = document.getElementById('speed');
const turnDisplay = document.getElementById('turn');
const statusDisplay = document.getElementById('status');

// Touch and mouse events for joystick
joystick.addEventListener('mousedown', startDrag);
joystick.addEventListener('touchstart', startDrag, {passive: false});

document.addEventListener('mousemove', drag);
document.addEventListener('touchmove', drag, {passive: false});

document.addEventListener('mouseup', stopDrag);
document.addEventListener('touchend', stopDrag);

function startDrag(e) {
    isDragging = true;
    joystick.style.transition = 'none';
    e.preventDefault();
}

function drag(e) {
    if (!isDragging) return;
    
    e.preventDefault();
    
    const rect = container.getBoundingClientRect();
    const centerX = rect.left + rect.width / 2;
    const centerY = rect.top + rect.height / 2;
    
    let clientX, clientY;
    if (e.type.includes('touch')) {
        clientX = e.touches[0].clientX;
        clientY = e.touches[0].clientY;
    } else {
        clientX = e.clientX;
        clientY = e.clientY;
    }
    
    let deltaX = clientX - centerX;
    let deltaY = clientY - centerY;
    
    // Limit movement to circle
    const maxDistance = 100;
    const distance = Math.sqrt(deltaX * deltaX + deltaY * deltaY);
    
    if (distance > maxDistance) {
        deltaX = (deltaX / distance) * maxDistance;
        deltaY = (deltaY / distance) * maxDistance;
    }
    
    currentX = deltaX;
    currentY = deltaY;
    
    joystick.style.transform = `translate(calc(-50% + ${deltaX}px), calc(-50% + ${deltaY}px))`;
    
    // Calculate speed and turn values
    speed = Math.max(Math.round(-deltaY / maxDistance * maxSpeed), 0); // Negative for forward
    turn = Math.round(deltaX / maxDistance * maxTurn);
    
    triggerSend = true;
}

function stopDrag() {
    if (!isDragging) return;
    
    isDragging = false;
    joystick.style.transition = 'all 0.3s ease';
    joystick.style.transform = 'translate(-50%, -50%)';
    
    currentX = 0;
    currentY = 0;
    speed = 0;
    turn = 0;
    
    triggerSend = true;
}

function updateDisplay() {
    speedDisplay.textContent = speed;
    turnDisplay.textContent = turn;
}

function sendControl() {
    fetch('/control', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/x-www-form-urlencoded',
        },
        body: `speed=${speed}&turn=${turn}`
    }).then(() => {
        updateDisplay();
    }).catch(err => console.error('Control error:', err));
}

const controlInterval = setInterval(() => {
    if (triggerSend) {
        if (speed !== prevSpeed || turn !== prevTurn) {
            sendControl();
            prevSpeed = speed;
            prevTurn = turn;
            triggerSend = false;
        }
    } else {
        // Send zero values when not dragging
        if (prevSpeed !== 0 || prevTurn !== 0) {
            speed = 0;
            turn = 0;
            sendControl();
            prevSpeed = 0;
            prevTurn = 0;
        }
    }
}, controlIntervalTime);

function emergencyStop() {
    fetch('/stop', {
        method: 'POST'
    }).then(() => {
        statusDisplay.textContent = 'EMERGENCY STOP ACTIVE';
        statusDisplay.style.color = '#ff4757';
        speed = 0;
        turn = 0;
        updateDisplay();
        setTimeout(() => {
            statusDisplay.textContent = 'Ready';
            statusDisplay.style.color = '';
        }, 3000);
    }).catch(err => console.error('Stop error:', err));
}

// Prevent scrolling on mobile
document.addEventListener('touchmove', function(e) {
    if (isDragging) {
        e.preventDefault();
    }
}, {passive: false});