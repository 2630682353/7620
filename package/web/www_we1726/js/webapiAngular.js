var REQUEST_CGI = '/cgi-bin/webapi?action=';
var DEFAULT_ENCRPTION = 'psk-mixed+ccmp';
var actionUrl = "http://" + document.domain + "/protocol.csp?";
var app = angular.module('webapiApp', []);

app.controller('loginCtrl', function ($scope, $http) {
    $('.loading').hide();
    $scope.txt_pwd = "";
    $scope.hide_msg = true;
    $scope.loginInfo = {"pwd": ""};
    //login
    $scope.onLogin = function () {
        //console.log('onLogin');
        if (angular.isUndefined($scope.txt_pwd)) {
            // alert('请输入密码！');
            var tipText = '请输入密码！';
            tipModel(tipText);
            return;
        }
        //    if($scope.txt_pwd.lenght<8)
        //            {
        //alert('inputs less than 8 ');
        //return;
        //            }
        $scope.loginInfo.pwd = $scope.txt_pwd;
        var pwd = SparkMD5.hashBinary('admin' + $scope.loginInfo.pwd);
        $http.post(actionUrl + "fname=system&opt=login&function=set&usrid=" + pwd)
                .success(function (ret) {
                    if (ret.error == 0) {
                    	$.cookie('lstatus', true);
                        window.location = 'state.html';
                    } else if (ret.error == '10001') {
                        window.location = 'invalidLogin.html';
                    }
                });
    };


    function ShowErrors(error, info1, info2) {
        $scope.hide_msg = false;
        $scope.errMsg = error;
    }
});


/*页脚信息*/
app.controller('footerCtrl', function ($rootScope,$http) {
    var sysInfo={
        devid:'...',
        sw_version:'...',
        mac_eth01:'...'
    }
    $rootScope.systeminfo=sysInfo;
    $http.post(actionUrl + 'fname=system&opt=firmstatus&function=get')
        .success(function (data) {
            $http.post(actionUrl + 'fname=net&opt=wan_conf&function=get')
                .success(function(wandata){
                    sysInfo.devid=data.model;
                    sysInfo.sw_version=data.localfirm_version;
                    sysInfo.mac_eth01=wandata.mac;
                    $rootScope.systeminfo=sysInfo;
                })
        })

})

/*检测是否首次配置接口*/
app.controller('firstCtrl', function ($scope, $http) {
    var password = 'admin';
    var firstPwd = {
        "pwd": password
    }
    ClearStorageCache();
    var str = SparkMD5.hashBinary('adminadmin');
    var url = actionUrl + 'fname=system&opt=login&function=set&usrid=' + str;
    $http.post(url)
            .success(function (res) {
                if (res.error == 0) {
                    $.cookie('lstatus', true);
                    $http.post(actionUrl + 'fname=system&opt=device_check&function=get')
                            .success(function (data) {
                                if (data.error == 0 && data.config_status == 0) {
                                    window.location.href = 'startconfig.html';
                                } else {
                                    window.location.href = 'login.html';
                                }
                            });
                } else {
                    window.location.href = 'login.html';
                }
            });
});

/*开始配置*/
app.controller('startCtrl', function ($scope) {
    $scope.detectionHref = 'detection.html';
});

/*wan口检测*/
app.controller('linkTipCtrl', function ($scope) {
    $scope.detectionHref = 'detection.html';
    $scope.setwifiHref = 'setwifi.html';
});

/*检测上网类型*/
app.controller('testCtrl', function ($scope, $http) {
    $.ajax({
        url: actionUrl + 'fname=net&opt=wan_info&function=get',
        type: 'POST',
        dataType: 'json',
        contentType: "application/json; charset=utf-8",
        success: function (data) {
            if (data.error == 0) {
                var wanPort = data.link;
                if (data.connected == 1) {
                    setTimeout(function(){
                        window.location.href = 'setwifi.html';
                    },2000)
                } else {
                	if(data.link == 0){
                		setTimeout(function(){
		                    window.location = 'linktip.html';
		                },2000)
                	}else{
                		setTimeout(function(){
	                        window.location.href = 'pppoe.html';
	                    },2000)
                	}      
                }
            } 
        },
        error: function (data) {
            setTimeout(function(){
                window.location = 'linktip.html';
            },2000)
        }
    })

})

/*pppoe设置*/
app.controller('pppoeCtrl', function ($scope, $http) {
    $scope.setwifiHref = 'setwifi.html';

    $("#ppppoeSubmit").click(function () {
        var pppoeName = $("#pppoeName").val();
        var pppoePwd = $("#pwdbox input").val();
        $scope.systeminfo = getSysinfo();
        var mac = $scope.systeminfo.mac_eth01;
        var data = "fname=net&opt=wan_conf&function=set&user=" + pppoeName + "&passwd=" + pppoePwd + "&mode=2&mac=" + mac;

        $http.post(actionUrl + data);
        //CheckWebActive($http, 'detection.html'); //detection.html => 检测上网类型
        CheckWebActive($http, 'setwifi.html'); //手动跳转下一步
    })
})


function CheckWebActive($http, links, msg) {
    msg?msg:'保存设置中。。。与路由器的连接可能会断开！';
    showLoading(msg);
    var check = window.setInterval(testInter, 5000);
    var delay = 0;
    //var isSuccess = false;
    function testInter() {
        if (delay++ > 2) {
            window.clearInterval(check);
            closeLoading();
            closeSetDialog();
            //var tipText = '设置成功，请检查是否连接到路由器，点击确定后，将自动重新检测！';
            if (links && (links!='null'||links!='NULL')) {
            	window.location.href = links;
                //tipModel(tipText, links);
            } else {
            	window.location.href = location;
                //tipModel(tipText, location);
            }
        }
        //if(delay++ < 1) return;
        //if (delay > 10) {
        //    window.clearInterval(check);
        //    alert('请检查网络是否连接正常！');
        //    closeSetDialog();
        //    closeLoading();
        //    return;
        //}
        /*检查服务是否准备好*/
        //var postJson = {'token': GetToken()};
        //$http.post(REQUEST_CGI + 'sysinfo_get', postJson)
        //    .success(function (ret) {
        //        if (ret.resp.err == 0 && !isSuccess) {
        //            console.log('CheckWebActive====');
        //            isSuccess = true;
        //            window.clearInterval(check);
        //            closeSetDialog();
        //            closeLoading();
        //            if(location){
        //                window.location.href = location;
        //            }
        //        }
        //    })
        //;
    }

}
/*显示设置结果*/
function showRet(ret) {
    closeLoading();
    var tipText;
    if (ret) {
        var retSuccess = isSuccess(ret);
        switch (retSuccess) {
            case RET_SUCCESS:
                closeSetDialog();
                // alert('设置成功！');
                tipText = '设置成功！';
                tipModel(tipText, location);
                break;
            case RET_ERR_MSG:
                // alert('设置失败：' + ret.resp.msg);
                tipText = '设置失败：' + ret.resp.msg;
                tipModel(tipText, location);
                break;
            case RET_INVALID_LOGIN:
                break;
            default :
                tipText = '设置失败：' + ret.error;
                tipModel(tipText, location);
                break;
        }
    } else {
        // alert('网络通讯失败，请检查网络或者重新提交！');
        tipText = '网络通讯失败，请检查网络或者重新提交！';
        tipModel(tipText, location);
    }
}
/*延时页面跳转
 * url：需要跳转的页面
 * delay：延时时长，单位：秒
 * */
function gotoDelay(url, delay) {
    var check = window.setInterval(testInter, 1000);
    var times = 0;

    function testInter() {
        if (times >= delay) {
            window.clearInterval(check);
            window.location.href = url;
        } else {
            times++;
        }
    }
}
function getSysinfo() {
    var sysinfo = {};
    var firmwareInfo = getFirmwareInfo();
    sysinfo.devid = firmwareInfo.devid;
    sysinfo.sw_version = firmwareInfo.sw_version;
    var url = "fname=net&opt=wan_conf&function=get";
    $.ajax({
        url: actionUrl + url,
        type: "post",
        async: false,
        dataType: "json",
        success: function (res) {
            if (res.error == 0) {
                sysinfo.mac_eth01 = res.rawmac;
            }
        }
    });
    sysinfo.routerName = '乐家路由宝';
    return sysinfo;
}

function getMainInfo() {
    var main = {};
    var url = 'fname=system&opt=main&function=get';
    $.ajax({
        url: actionUrl + url,
        type: 'post',
        async: false,
        dataType: 'json',
        success: function (res) {
            if (res.error == 0) {
                main.stime = res.ontime;
            }
        }
    });
    return main;
}

function getWanInfo() {
    var wanInfo = {};
    var url = 'fname=system&opt=wan_info&function=get';
    $.ajax({
        url: actionUrl + url,
        type: "post",
        async: false,
        dataType: "json",
        success: function (res) {
            if (res.error == 0) {
                wanInfo.link = res.link;
                wanInfo.connected = res.connected;
                wanInfo.ip = res.ip;
            }
        }
    });
    return wanInfo;
}


function getFirmwareInfo() {
    var info = {};
    var url = 'fname=system&opt=firmstatus&function=get';
    $.ajax({
        url: actionUrl + url,
        type: "post",
        async: false,
        dataType: "json",
        success: function (res) {
            if (res.error == 0) {
                info.devid = res.model;
                info.sw_version = res.localfirm_version;
                info.port_status = res.port_status;
            }
        }
    });
    return info;
}

