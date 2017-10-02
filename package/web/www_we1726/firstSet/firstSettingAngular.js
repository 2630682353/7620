/**
 * Created by lad on 2016/6/3.
 */

app.controller('wifiCtrl', function ($scope, $http) {
    $http.post(actionUrl + 'fname=net&opt=wifi_ap&function=get')
            .success(function (ret) {
                if (ret.error == 0) {
                    $scope.content = ret;
                    $scope.ssid = ret.ssid;
                    $scope.wifiPwd = ret.passwd;
                    //$scope.wifiPwd = '';
//                    $scope.oldPwd = $scope.wifiPwd;
                } else {
                    tipModel(ret.error);
                }
            })
            .error(function (ret) {
                alert('网络错误，请刷新当前页面！');
            });
    /*清空wifi名称输入框内容*/
    $scope.clearWifiName = function () {
        //console.log('clearWifiName');
        $scope.ssid = '';
        $('#wifiName').focus();
    }
    /*显示wifi密码*/
    $scope.showPwd = function () {
        $scope.pwdShow ^= 1;
    }
    /*提交设置*/
    $scope.toggleSubmit = function () {
        //console.log('toggleSubmit');
        if ($scope.ssid.length<1) {
            tipModel('请输入无线名称！');
            return;       
        }
        if($scope.ssid.length > 31 ||encodeURI($scope.ssid).indexOf("%20")!="-1"|| /[\':;*?~`!@#$%^&+={}\[\]\<\\(\),\.\。\，]/.test($scope.ssid)){
            tipModel('无线名称长度不得超过31位字符或者包含特殊字符！');
            return;       
        }
        if ($scope.wifiPwd.length > 31 || ($scope.wifiPwd.length > 0 && $scope.wifiPwd.length < 8)) {
            //alert('密码长度必须大于等于8位，且不能超过31位！');
            tipModel('密码长度必须大于等于8位，且不能超过31位！');
            return;
        }
        if (escape($scope.wifiPwd).indexOf("%u") != -1 ||encodeURI($scope.wifiPwd).indexOf("%20")!="-1"|| /[\'\"{}\[\]]/.test($scope.wifiPwd)) {
            tipModel('密码不能包含中文字符或者特殊字符！');
            return;       
        }

        var data = $scope.ssid + '&passwd=' + $scope.wifiPwd;
        $http.post(actionUrl + 'fname=net&opt=wifi_ap&function=set&ssid=' + data)
                .success(function (data) {
                    if (data.error == 0) {
                        var tipText = '设置成功！';
                        tipModel(tipText, 'login.html');
                        $scope.wifiName = data.ssid;
                        var wifiInfo = data;
                        $scope.$broadcast('wifiInfo', wifiInfo);
                    } else {
                        var tipText = 'err:' + data.error;
                        tipModel(tipText);
                    }
                })



    }
});