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
        /* 样式保持不变 */
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
            background: linear-gradient(135deg, #3498db, #3498db);
            padding: 15px 20px;
            border-bottom: 2px solid rgba(255, 255, 255, 0.1);
            color: white;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        
        .header-title {
            margin: 0;
            font-size: 1.5rem;
        }
        
        .ip-container {
            font-size: 0.9rem;
        }
        
        .card-body {
            padding: 25px;
        }
        
        .progress-container {
            margin: 25px 0;
            background: #e0e0e0;
            border-radius: 10px;
            overflow: hidden;
            height: 30px;
            border: 1px solid rgba(0, 0, 0, 0.1);
            position: relative;
        }
        
        .progress-bar {
            height: 100%;
            display: flex;
            align-items: center;
            justify-content: center;
            font-weight: bold;
            transition: width 0.3s ease;
            background: linear-gradient(90deg, #3498db, #2ecc71);
        }
        
        .progress-text {
            position: absolute;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            display: flex;
            align-items: center;
            justify-content: center;
            font-weight: bold;
            z-index: 10;
            pointer-events: none;
            color: #333;
        }
        
        .status-card {
            border: 1px solid rgba(0, 0, 0, 0.1);
            border-radius: 10px;
            padding: 15px;
            margin-top: 20px;
            background: rgba(245, 245, 245, 0.7);
        }
        
        .status-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 15px;
        }
        
        .status-item {
            display: flex;
            justify-content: space-between;
            margin: 8px 0;
        }
        
        .btn-primary {
            background: linear-gradient(to bottom, #3498db, #3498db);
            border: none;
            transition: all 0.3s;
            width: 100%;
            padding: 12px;
            font-weight: bold;
            letter-spacing: 1px;
            box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
        }
        
        .btn-primary:hover {
            background: linear-gradient(to bottom, #2980b9, #1c6ca4);
            transform: translateY(-2px);
            box-shadow: 0 6px 12px rgba(0, 0, 0, 0.15);
        }
        
        .btn-primary:disabled {
            background: #95a5a6 !important;
            cursor: not-allowed;
            transform: none !important;
            box-shadow: none !important;
        }
        
        .device-info {
            display: flex;
            justify-content: space-around;
            text-align: center;
            margin-top: 20px;
            padding-top: 15px;
            border-top: 1px solid rgba(0, 0, 0, 0.1);
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
            background: rgba(231, 76, 60, 0.1);
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
            font-size: 0.9rem;
            background: rgba(0, 0, 0, 0.15);
            padding: 4px 8px;
            border-radius: 4px;
            display: inline-block;
            color: white;
            cursor: pointer;
        }
        
        .ip-label {
            margin-right: 8px;
            opacity: 0.8;
        }
        
        .md5-label {
            font-size: 0.8rem;
            color: #666;
        }
        
        .md5-value {
            font-family: monospace;
            font-size: 0.75rem;
            word-break: break-all;
            color: #2980b9;
            text-align: right;
        }
    </style>
</head>
<body>
    <div class="card">
        <div class="card-header">
            <h2 class="header-title">OTA在线升级系统</h2>
            <div class="ip-container">
                <span class="ip-label">设备IP:</span>
                <span class="ip-address" id="deviceIP">连接中...</span>
            </div>
        </div>
        
        <div class="card-body">
            <div class="instructions">
                <strong>⚠️ 警告:</strong> 更新过程中请勿断开设备电源或网络连接！
            </div>
            
            <form id="uploadForm" class="mb-4">
                <div class="mb-3">
                    <label for="firmwareFile" class="form-label">选择固件文件（.bin格式）</label>
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
                <div class="status-header">
                    <h5 class="mb-0">系统状态</h5>
                    <div class="md5-label">固件MD5: <span id="firmwareMD5" class="md5-value">N/A</span></div>
                </div>
                
                <div class="status-item">
                    <span>更新进度：</span>
                    <span id="uploadPercent">0%</span>
                </div>
                <div class="status-item">
                    <span>系统状态：</span>
                    <span id="statusText">就绪</span>
                </div>
            </div>
            
            <div class="device-info">
                <div class="info-box">
                    <div class="info-value" id="fwVersion">N/A</div>
                    <div class="info-label">固件版本</div>
                </div>
                <div class="info-box">
                    <div class="info-value" id="flashSize">N/A</div>
                    <div class="info-label">可用空间</div>
                </div>
                <div class="info-box">
                    <div class="info-sn" id="SNID">N/A</div>
                    <div class="info-label">序列号</div>
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
                throw new Error('无法连接到设备');
            }
            
            const data = await response.json();
            
            // 更新设备信息
            document.getElementById('fwVersion').textContent = data.version || 'N/A';
            document.getElementById('flashSize').textContent = 
                data.freeFlash ? `${data.freeFlash} KB` : 'N/A';
            document.getElementById('SNID').textContent = data.SNID || 'N/A';
            document.getElementById('firmwareMD5').textContent = data.firmwareMD5 || 'N/A';
            
            // 显示设备IP地址
            const ipAddress = data.ipAddress;
            const ipElement = document.getElementById('deviceIP');
            if (ipAddress) {
                ipElement.textContent = ipAddress;
                
                // 复制功能
                ipElement.title = "点击复制IP地址";
                ipElement.addEventListener('click', function() {
                    navigator.clipboard.writeText(ipAddress);
                    const originalText = ipElement.textContent;
                    ipElement.textContent = '已复制!';
                    setTimeout(() => {
                        ipElement.textContent = originalText;
                    }, 2000);
                });
            } else {
                ipElement.textContent = '未获取到IP';
            }
            
        } catch (error) {
            console.error('获取设备信息失败:', error);
            document.getElementById('statusText').textContent = '连接错误';
            document.getElementById('statusText').style.color = '#e74c3c';
            
            // 尝试直接获取当前访问的IP
            const currentHost = window.location.hostname;
            if (currentHost && currentHost !== 'esp32.local') {
                document.getElementById('deviceIP').textContent = currentHost;
            } else {
                document.getElementById('deviceIP').textContent = '无法获取IP';
            }
            
            // 设置重试机制
            setTimeout(fetchDeviceInfo, 5000);
        }
    }
    
    // 更新进度显示
    function updateProgress(uploadPercent) {
        uploadPercent = Math.max(0, Math.min(100, uploadPercent));
        document.getElementById('uploadPercent').textContent = `${uploadPercent}%`;
        const progressBar = document.getElementById('progressBar');
        progressBar.style.width = `${uploadPercent}%`;
        document.getElementById('progressText').textContent = `${uploadPercent}%`;
        
        const statusText = document.getElementById('statusText');
        if (uploadPercent < 100) {
            statusText.textContent = '上传中...';
            statusText.style.color = '';
            progressBar.style.background = 'linear-gradient(90deg, #3498db, #2ecc71)';
        } else {
            statusText.textContent = '正在写入设备...';
            statusText.style.color = '#f39c12';
            progressBar.style.background = 'linear-gradient(90deg, #f39c12, #e67e22)';
            progressBar.classList.add('flashing');
        }
    }
    
    // 表单提交处理
    document.getElementById('uploadForm').addEventListener('submit', async function(e) {
        e.preventDefault();
        
        const btn = document.getElementById('updateBtn');
        const btnText = document.getElementById('btnText');
        const spinner = document.getElementById('btnSpinner');
        const fileInput = document.getElementById('firmwareFile');
        const statusText = document.getElementById('statusText');
        const progressBar = document.getElementById('progressBar');
        
        // 重置进度
        progressBar.classList.remove('flashing');
        updateProgress(0);
        
        // 检查文件是否已选择
        if (!fileInput.files || fileInput.files.length === 0) {
            statusText.textContent = '未选择文件';
            statusText.style.color = '#e74c3c';
            return;
        }
        
        // 检查文件扩展名
        const fileName = fileInput.files[0].name.toLowerCase();
        if (!fileName.endsWith('.bin')) {
            statusText.textContent = '仅支持.bin文件';
            statusText.style.color = '#e74c3c';
            return;
        }
        
        // 获取文件大小
        const file = fileInput.files[0];
        const fileSize = file.size;
        
        // 创建XHR对象
        const xhr = new XMLHttpRequest();
        const formData = new FormData();
        formData.append('update', file);
        
        // 创建包含文件大小的URL
        const url = `/update?fileSize=${fileSize}`;
        
        // 禁用按钮并显示加载状态
        btn.disabled = true;
        btnText.textContent = '更新中...';
        spinner.classList.remove('d-none');
        statusText.textContent = '开始上传';
        statusText.style.color = '';
        
        // 上传进度处理
        xhr.upload.addEventListener('progress', function(evt) {
            // 使用fileSize而不是evt.total，因为evt.total可能包含额外数据
            const percent = Math.round((evt.loaded / fileSize) * 100);
            updateProgress(percent);
        });
        
        // 请求完成处理
        xhr.addEventListener('load', async function() {
            if (xhr.status === 200) {
                updateProgress(100);

                statusText.style.color = '#2ecc71';
                progressBar.style.background = '#2ecc71';
                progressBar.classList.remove('flashing');
                
                let countdown = 3;
                const countdownInterval = setInterval(() => {
                    btnText.textContent = `重启中...(${countdown})`;
                    if (countdown <= 0) {
                        clearInterval(countdownInterval);
                        btnText.textContent = '请手动刷新后操作';
                        setTimeout(fetchDeviceInfo, 3000);
                    }
                    countdown--;
                }, 1000);
            } else {
                btn.disabled = false;
                btnText.textContent = '重试更新';
                spinner.classList.add('d-none');
                statusText.textContent = '更新失败';
                statusText.style.color = '#e74c3c';
                progressBar.style.background = '#e74c3c';
            }
        });
        
        // 错误处理
        xhr.addEventListener('error', function() {
            btn.disabled = false;
            btnText.textContent = '重试更新';
            spinner.classList.add('d-none');
            statusText.textContent = '连接错误';
            statusText.style.color = '#e74c3c';
        });
        
        // 请求超时处理
        xhr.timeout = 300000; // 5分钟超时
        xhr.ontimeout = function() {
            btn.disabled = false;
            btnText.textContent = '重试更新';
            spinner.classList.add('d-none');
            statusText.textContent = '请求超时';
            statusText.style.color = '#e74c3c';
        };
        
        // 发送请求
        xhr.open('POST', url, true);
        xhr.send(formData);
    });
    
    // 初始化页面时获取设备信息
    document.addEventListener('DOMContentLoaded', fetchDeviceInfo);
    
    // 每10秒刷新一次设备信息
    setInterval(fetchDeviceInfo, 10000);
</script>
</body>
</html>
)=====";