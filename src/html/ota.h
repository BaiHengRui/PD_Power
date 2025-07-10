const char* serverIndex = 
R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>OTA Update</title>
    <link href="https://cdn.jsdelivr.net/npm/bootstrap@5.3.0/dist/css/bootstrap.min.css" rel="stylesheet">
    <style>
        :root {
            --primary: #4a86e8;
            --primary-light: #6da6ff;
            --success: #34c759;
            --warning: #ff9500;
            --danger: #ff3b30;
            --background: #f8f9fa;
            --card-bg: rgba(255, 255, 255, 0.95);
            --text: #333333;
            --text-light: #666666;
            --border: #e0e0e0;
        }
        
        body {
            background: linear-gradient(135deg, #e6f0ff, #f0f5ff, #e6f0ff);
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
            padding: 20px;
            font-family: 'Segoe UI', 'PingFang SC', 'Microsoft YaHei', sans-serif;
            color: var(--text);
            margin: 0;
        }
        
        .card {
            background: var(--card-bg);
            backdrop-filter: blur(10px);
            border-radius: 15px;
            box-shadow: 0 12px 30px rgba(0, 0, 0, 0.08);
            overflow: hidden;
            width: 100%;
            max-width: 500px;
        }
        
        .card-header {
            background: rgba(52, 152, 219, 0.9);
            padding: 20px;
            text-align: center;
            border-bottom: 2px solid rgba(255, 255, 255, 0.1);
        }
        
        .card-body {
            padding: 25px;
        }
        
        .progress-container {
            margin: 25px 0;
            background:rgb(164, 206, 249);
            border-radius: 10px;
            overflow: hidden;
            height: 30px;
            border: 1px solid rgba(255, 255, 255, 0.9);
            position: relative; /* 添加相对定位 */
        }
        
        .progress-bar {
            height: 100%;
            display: flex;
            align-items: center;
            justify-content: center;
            font-weight: bold;
            transition: width 0.3s ease;
            background-color: var(--primary);
        }
        
        .progress-text {
            position: absolute; /* 绝对定位 */
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            display: flex;
            align-items: center;
            justify-content: center;
            font-weight: bold;
            z-index: 10; /* 确保文字在进度条上方 */
            pointer-events: none; /* 避免干扰点击事件 */
        }
        
        .status-card {
            border: 1px solid rgba(36, 36, 36, 0.9);
            border-radius: 10px;
            padding: 15px;
            margin-top: 20px;
        }
        
        .status-item {
            display: flex;
            justify-content: space-between;
            margin: 8px 0;
        }
        
        .btn-primary {
            background: var(--primary);
            border: none;
            transition: all 0.3s;
            width: 100%;
            padding: 12px;
            font-weight: bold;
            letter-spacing: 1px;
        }
        
        .btn-primary:hover {
            background: #2980b9;
            transform: translateY(-2px);
            box-shadow: 0 5px 15px rgba(0, 0, 0, 0.2);
        }
        
        .btn-primary:disabled {
            background: #7f8c8d;
            transform: none;
            box-shadow: none;
        }
        
        .device-info {
            display: flex;
            justify-content: space-around;
            text-align: center;
            margin-top: 20px;
            padding-top: 15px;
            border-top: 1px solid rgba(255, 255, 255, 0.1);
        }
        
        .info-box {
            padding: 10px;
        }
        
        .info-value {
            font-size: 1.2rem;
            font-weight: bold;
            color: var(--primary);
        }
        
        .info-sn {
            font-size: 1.2rem;
            font-weight: bold;
            color: var(--primary);
            text-transform: lowercase;
        }
        .info-label {
            font-size: 0.85rem;
            opacity: 0.8;
        }
        
        .flashing {
            animation: pulse 1.5s infinite;
        }
        
        @keyframes pulse {
            0% { opacity: 0.6; }
            50% { opacity: 1; }
            100% { opacity: 0.6; }
        }
        
        .instructions {
            background: rgba(231, 76, 60, 0.2);
            border-left: 4px solid var(--danger);
            padding: 10px 15px;
            margin: 20px 0;
            border-radius: 0 5px 5px 0;
        }
        
        .spinner-border {
            width: 1rem;
            height: 1rem;
            vertical-align: text-bottom;
            border: .25em solid currentColor;
            border-right-color: transparent;
            border-radius: 50%;
            animation: spinner-border .75s linear infinite;
        }
        
        @keyframes spinner-border {
            to { transform: rotate(360deg); }
        }
        
        .d-none {
            display: none;
        }
        
        .ip-address {
            font-family: monospace;
            font-size: 1.1rem;
            background: rgba(0, 0, 0, 0.2);
            padding: 5px 10px;
            border-radius: 5px;
            display: inline-block;
            margin-top: 5px;
        }
    </style>
</head>
<body>
    <div class="card">
        <div class="card-header">
            <h2 class="mb-0">OTA在线升级系统</h2>
            <div class="text-muted">IP Address:</div>
            <div class="ip-address" id="deviceIP">Connecting...</div>
        </div>
        
        <div class="card-body">
            <div class="instructions">
                <strong>⚠️ 警告:</strong> 更新期间不要断开电源！
            </div>
            
            <form id="uploadForm" class="mb-4">
                <div class="mb-3">
                    <label for="firmwareFile" class="form-label">选择固件文件（.bin）</label>
                    <input class="form-control" type="file" id="firmwareFile" name="update" accept=".bin" required>
                </div>
                <button type="submit" class="btn btn-primary" id="updateBtn">
                    <span id="btnText">开始更新</span>
                    <span id="btnSpinner" class="spinner-border spinner-border-sm d-none"></span>
                </button>
            </form>
            
            <div class="progress-container">
                <div id="progressBar" class="progress-bar"></div>
                <div id="progressText" class="progress-text">0%</div>
            </div>
            
            <div class="status-card">
                <h5 class="mb-3">系统状态</h5>
                <div class="status-item">
                    <span>更新进度：</span>
                    <span id="uploadPercent">0%</span>
                </div>
                <div class="status-item">
                    <span>刷写进度：</span>
                    <span id="flashPercent">0%</span>
                </div>
                <div class="status-item">
                    <span>系统状态</span>
                    <span id="statusText">Ready</span>
                </div>
            </div>
            
            <div class="device-info">
                <div class="info-box">
                    <div class="info-value" id="fwVersion">N/A</div>
                    <div class="info-label">版本</div>
                </div>
                <div class="info-box">
                    <div class="info-value" id="flashSize">N/A</div>
                    <div class="info-label">可用空间</div>
                </div>
                <div class="info-box">
                    <div class="info-sn" id="SNID">N/A</div>
                    <div class="info-label">SN</div>
                </div>
            </div>
        </div>
    </div>

    <script>
        // 获取设备信息
        async function fetchDeviceInfo() {
            try {
                const response = await fetch('/info');
                if (!response.ok) {
                    throw new Error('Network response was not ok');
                }
                
                const data = await response.json();
                
                document.getElementById('fwVersion').textContent = data.version || 'N/A';
                document.getElementById('flashSize').textContent = 
                    data.freeFlash ? `${data.freeFlash} KB` : 'N/A';
                document.getElementById('SNID').textContent = data.SNID || 'N/A';
                
                // 显示设备IP地址
                const ipAddress = data.ipAddress;
                const ipElement = document.getElementById('deviceIP');
                if (ipAddress) {
                    ipElement.textContent = ipAddress;
                    ipElement.style.color = '#2ecc71'; // 绿色表示成功获取
                    
                    // 添加复制功能
                    ipElement.title = "Click to copy IP address";
                    ipElement.style.cursor = 'pointer';
                    ipElement.addEventListener('click', function() {
                        navigator.clipboard.writeText(ipAddress);
                        const originalText = ipElement.textContent;
                        ipElement.textContent = 'Copied!';
                        setTimeout(() => {
                            ipElement.textContent = originalText;
                        }, 2000);
                    });
                } else {
                    ipElement.textContent = 'Not available';
                    ipElement.style.color = '#e74c3c'; // 红色表示错误
                }
                
            } catch (error) {
                console.error('Failed to fetch device info:', error);
                document.getElementById('statusText').textContent = 'Info Error';
                document.getElementById('statusText').style.color = '#e74c3c';
                
                // 尝试直接获取当前访问的IP
                const currentHost = window.location.hostname;
                if (currentHost && currentHost !== 'esp32.local') {
                    document.getElementById('deviceIP').textContent = currentHost;
                } else {
                    document.getElementById('deviceIP').textContent = 'Unable to get IP';
                    document.getElementById('deviceIP').style.color = '#e74c3c';
                }
            }
        }
        
        // 获取刷写进度
        async function fetchFlashProgress() {
            try {
                const response = await fetch('/progress');
                if (!response.ok) return 0;
                
                const progress = await response.text();
                return parseInt(progress) || 0;
            } catch (error) {
                return 0;
            }
        }
        
        // 更新进度显示
        function updateProgress(uploadPercent, flashPercent) {
            // 限制百分比在0-100范围内
            uploadPercent = Math.max(0, Math.min(100, uploadPercent));
            flashPercent = Math.max(0, Math.min(100, flashPercent));
            
            // 更新上传进度
            const uploadPercentElement = document.getElementById('uploadPercent');
            uploadPercentElement.textContent = `${uploadPercent}%`;
            
            // 更新刷写进度
            const flashPercentElement = document.getElementById('flashPercent');
            flashPercentElement.textContent = `${flashPercent}%`;
            
            // 计算总进度 (上传40% + 刷写60%)
            const totalProgress = Math.floor((uploadPercent * 0.4) + (flashPercent * 0.6));
            
            // 更新进度条
            const progressBar = document.getElementById('progressBar');
            progressBar.style.width = `${totalProgress}%`;
            
            // 更新进度文本（固定在容器中间）
            document.getElementById('progressText').textContent = `${totalProgress}%`;
            
            // 更新状态文本
            const statusText = document.getElementById('statusText');
            
            if (uploadPercent < 100) {
                statusText.textContent = 'Uploading...';
                progressBar.style.backgroundColor = '#3498db';
            } else if (flashPercent < 100) {
                statusText.textContent = 'Flashing...';
                progressBar.style.backgroundColor = '#f39c12';
                progressBar.classList.add('flashing');
            } else if (flashPercent === 100) {
                statusText.textContent = 'Update Complete!';
                progressBar.style.backgroundColor = '#2ecc71';
                progressBar.classList.remove('flashing');
            }
        }
        
        // 处理表单提交
        document.getElementById('uploadForm').addEventListener('submit', function(e) {
            e.preventDefault();
            
            const btn = document.getElementById('updateBtn');
            const btnText = document.getElementById('btnText');
            const spinner = document.getElementById('btnSpinner');
            const fileInput = document.getElementById('firmwareFile');
            
            // 检查文件是否已选择
            if (!fileInput.files || fileInput.files.length === 0) {
                document.getElementById('statusText').textContent = 'No file selected';
                document.getElementById('statusText').style.color = '#e74c3c';
                return;
            }
            
            // 检查文件扩展名
            const fileName = fileInput.files[0].name.toLowerCase();
            if (!fileName.endsWith('.bin')) {
                document.getElementById('statusText').textContent = 'Invalid file type (.bin only)';
                document.getElementById('statusText').style.color = '#e74c3c';
                return;
            }
            
            // 禁用按钮并显示加载状态
            btn.disabled = true;
            btnText.textContent = 'Updating...';
            spinner.classList.remove('d-none');
            
            const formData = new FormData(this);
            const xhr = new XMLHttpRequest();
            
            // 上传进度处理
            xhr.upload.addEventListener('progress', function(evt) {
                if (evt.lengthComputable) {
                    const percent = Math.round((evt.loaded / evt.total) * 100);
                    updateProgress(percent, 0);
                }
            });
            
            // 定期获取刷写进度
            let flashProgressInterval;
            
            xhr.addEventListener('loadstart', function() {
                flashProgressInterval = setInterval(async () => {
                    const flashPercent = await fetchFlashProgress();
                    const uploadPercent = parseInt(document.getElementById('uploadPercent').textContent) || 0;
                    
                    // 防止上传完成后的进度回退
                    if (uploadPercent === 100) {
                        updateProgress(100, flashPercent);
                    }
                }, 500);
            });
            
            // 请求完成处理
            xhr.addEventListener('load', function() {
                clearInterval(flashProgressInterval);
                
                if (xhr.status === 200) {
                    // 确保进度显示100%
                    updateProgress(100, 100);
                    document.getElementById('statusText').textContent = 'Rebooting...';
                    
                    // 显示重启倒计时
                    let countdown = 3;
                    const countdownInterval = setInterval(() => {
                        btnText.textContent = `Rebooting in ${countdown}s`;
                        if (countdown <= 0) {
                            clearInterval(countdownInterval);
                            btnText.textContent = 'Rebooting Now';
                            
                            // 锁定进度显示为100%
                            document.getElementById('progressText').textContent = "100%";
                            document.getElementById('progressBar').style.width = "100%";
                            document.getElementById('uploadPercent').textContent = "100%";
                            document.getElementById('flashPercent').textContent = "100%";
                        }
                        countdown--;
                    }, 1000);
                } else {
                    btn.disabled = false;
                    btnText.textContent = 'Retry Update';
                    spinner.classList.add('d-none');
                    document.getElementById('statusText').textContent = 'Update Failed';
                    document.getElementById('statusText').style.color = '#e74c3c';
                    document.getElementById('progressBar').style.backgroundColor = '#e74c3c';
                }
            });
            
            // 错误处理
            xhr.addEventListener('error', function() {
                clearInterval(flashProgressInterval);
                btn.disabled = false;
                btnText.textContent = 'Retry Update';
                spinner.classList.add('d-none');
                document.getElementById('statusText').textContent = 'Connection Error';
                document.getElementById('statusText').style.color = '#e74c3c';
            });
            
            xhr.open('POST', '/update', true);
            xhr.send(formData);
        });
        
        // 初始化页面时获取设备信息
        document.addEventListener('DOMContentLoaded', fetchDeviceInfo);
    </script>
</body>
</html>
)=====";