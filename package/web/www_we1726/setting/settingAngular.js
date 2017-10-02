/**
 * Created by lad on 2016/6/1.
 */

/*系统设置*/
app.controller('systemCtrl', function ($scope, $http) {
    //$scope.systeminfo = getSysinfo();  //旧版 现已迁移至footerCtrl控制器下
    $scope.sysmain = getMainInfo();
    urlHref($scope);
    var uptime = $scope.sysmain.stime;

    var strTime;
    window.setInterval(function () {
        $("#uptimeMod").text(formatTime(uptime));
        uptime++
    }, 1000);
    function formatTime(uptime) {
        strTime = parseInt(uptime / 86400) + '天' + parseInt(uptime / 60 / 60 % 24) + '时' + parseInt(uptime / 60 % 60) + '分' + parseInt(uptime % 60) + '秒'
        return strTime;
    }

    $scope.updateVersion = '已经是最新版本';

    /*检查是否有新版本*/
    $http.post(actionUrl + 'fname=system&opt=firmstatus&function=get')
            .success(function (ret) {
                if (ret.error == 0) {
                    if (ret.newfirm_version) {
                        $scope.updateVersion = '发现新的版本，请点击在线升级进行版本更新！';
                        /*执行在线升级*/
                        $scope.updateOnline = function () {
                            $http.post(actionUrl + 'fname=system&opt=firmstatus&function=set&flag=get');
                            showLoading('更新中，请不要断开电源，大概需要5分钟，更新完成后，请重新连接设备,并且手动刷新当前页面！');
                        }
                    } else {
                        $scope.updateOnline = function () {
                            var tipText = '已经是最新版本，无需升级';
                            tipModel(tipText);
                        }
                    }
                }
            });


    /*恢复出厂设置*/
    $scope.restoreFactory = function () {
        //console.log('restoreFactory');
        showSetDialog('routerReset');
    }

    //修改恢复出厂设置提示 20160830
    $scope.submitReset = function () {
        $http.post(actionUrl + 'fname=system&opt=setting&action=default&function=set');
        //CheckWebActive($http, '/');
        showLoading('正在恢复出厂设置。。。与路由器的连接可能会断开！');
        var check = window.setInterval(testInter, 5000);
        var delay = 0;
        function testInter() {
            if (delay++ > 2) {
                window.clearInterval(check);
                closeLoading();
                closeSetDialog();
                tipModel('恢复出厂设置成功，请检查是否连接到路由器，点击确定后，将自动重新检测！', "/");
            }
        }
    }
    /*增加白名单*/
    $scope.addWhiteName = function () {
        showSetDialog('routerSetMac');
    }
    /*安装升级包*/
    //$(".upFile").on("change", "input[type='file']", function () {
    $(".upFile").on("change", "#uploadimg", function () {
        var blobSlice = File.prototype.slice || File.prototype.mozSlice || File.prototype.webkitSlice;
        if (!('FileReader' in window) || !('File' in window) || !blobSlice) {
            var tipText = '当前浏览器不支持，请使用火狐或谷歌浏览器！';
            tipModel(tipText);
            return false;
        }
        var filePath = $(this).val();
        var formData = new FormData($("#uploadForm")[0]);
        var files = $('input[name="uploadimg"]').prop('files');
        var fileSize = files[0].size;
        if(filePath.substr(-12) != "firmware.bin"){
        	$(".showFileName").html("").hide();
            $(".fileerrorTip").html("固件文件名错误！应为：firmware.bin").show();
        } else if (filePath.indexOf("bin") == -1) {
            $(".showFileName").html("").hide();
            $(".fileerrorTip").html("上传文件类型有误！请选择'.bin'文件").show();
            return false;
        } else if (fileSize <= 7168) {
            $(".showFileName").html("").hide();
            $(".fileerrorTip").html("请选择小于7M的文件").show();
            return false;
        } else {
        	
        	//解决onchange事件只触发一次bug
            if ($('#uploadimg')) {
                $('#uploadimg').replaceWith('<input id="uploadimg" type="file" name="uploadimg" accept=".bin">');
            }
            showLoading('正在上传……');
            $(".fileerrorTip").html("").hide();
            var arr = filePath.split('\\');
            var fileName = arr[arr.length - 1];
            $('#fileName').text(fileName);
            var reader = new FileReader();//新建一个FileReader
            reader.readAsBinaryString(files[0]);

            reader.onload = function (evt) { //读取完文件之后会回来这里
                var fileString = evt.target.result;
                var fileMd5 = SparkMD5.hashBinary(fileString);
                //console.log('md5=', fileMd5);
                $.ajax({
                    url: '/upload.csp?uploadpath=/tmp/ioos&t=firmware.bin',
                    type: 'POST',
                    data: formData,
                    async: false,
                    cache: false,
                    contentType: false,
                    processData: false,
                    beforeSend: function (data) {
                    },
                    success: function (data) {
                        closeLoading();
                        showSetDialog('fileUpModel');
                        var postJson = {};
                        postJson.token = GetToken();
                        postJson.content = {"name": fileName, "md5": fileMd5};
                        $scope.doUpload = function () {
                            $http.post(actionUrl + 'fname=system&opt=local_firmup&function=set', postJson)
                                    .success(function (data) {
                                        showLoading('更新中，请不要断开电源，大概需要5分钟，更新完成后，请重新连接设备,并且手动刷新当前页面！');
                                    })
                        }


                    },
                    error: function (data) {
                        var tipText = '网络错误，请重新上传！';
                        tipModel(tipText);
                    }
                });
            }
            reader.onerror = function () {
                var tipText = '读取文件失败，请重新选择上传的文件！';
                tipModel(tipText);
            }
        }
    })


})

