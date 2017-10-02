/**
 * Created by lad on 2016/6/1.
 */

app.controller('wifiDialogCtrl', function ($scope, $http) {

    $scope.channels = ['自动', 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13];
    $scope.HTmodels = ['HT20(兼容模式)', 'HT20/HT40'];

    $scope.guestWifi = {};
    $scope.guestWifi.ssid = 'Guest';
    $scope.guestWifi.hidden = 0;


    $scope.$on('wifiInfo', function (event, wifiInfo, guestwifiInfo) {
        var htMode = wifiInfo.bw;
        var mainWifi = wifiInfo;
        //console.log('wifiinfo', wifiInfo, mainWifi, guestWifi);
        $scope.wifiChannel = mainWifi.channel==0?"自动":mainWifi.channel;

        htMode = parseInt(htMode);
        $scope.HTmodel = $scope.HTmodels[htMode];

        if (angular.isDefined(mainWifi)) {
            $scope.mainWifiSSID = mainWifi.ssid;
            $scope.mainWifiPasswd = mainWifi.passwd;

            if (mainWifi.hidden == '1') {
                $scope.hideMainWifi = 1;
            } else {
                $scope.hideMainWifi = 0;
            }
        }

    });
    $scope.$on('guestwifiInfo',function(event,guestwifiInfo){
        var guestWifi = guestwifiInfo;
        
        if (angular.isDefined(guestWifi)) {
            $scope.guestWifi = guestWifi;
            $scope.guestWifiSwitch = guestWifi.guestWifiSwitch;
            $scope.guestWifiSSID = guestWifi.vssid;
            if (guestWifi.hidden == '1') {
                $scope.guestWifiHide = 1;
            } else {
                $scope.guestWifiHide = 0;
            }
            $scope.guestWifiPasswd = guestWifi.password;
        }
    })

    /*显示密码*/
    $scope.toggleShowPasswd = function (index) {
        switch (index) {
            case 0:
                $scope.mainPasswdHide ^= 1;
                break;
            case 1:
                $scope.guestPasswdHide ^= 1;
                break;
            default :
                alert('传入错误的参数，请联系开发人员！');
                break;
        }
    }
    /*隐藏网络*/
    $scope.toggleHideWifi = function (index) {
        switch (index) {
            case 0:
                $scope.hideMainWifi ^= 1;
                break;
            case 1:
                $scope.guestWifiHide ^= 1;
                break;
            default :
                alert('传入错误的参数，请联系开发人员！');
                break;
        }
    }
    /*高级设置菜单*/
    $scope.toggleMainAdvance = function () {
        $(".wifiSeniorUl").slideToggle();
        $(".wifiSeniorUl").find('span').toggleClass('seniorup');
    }
    /*访客网络开关*/
    $scope.toggleGuestSwitch = function () {
        $scope.guestWifiSwitch ^= 1;
        if (!angular.isDefined($scope.guestWifiSSID)) {
            $scope.guestWifiSSID = $scope.guestWifi.ssid;
            $scope.guestWifiHide = $scope.guestWifi.hidden;
        }
    }
    /*保存设置*/
    $scope.toggleSubmitWifi = function () {
        if (angular.isDefined($scope.mainWifiSSID)) {
            if ($scope.mainWifiSSID.length < 1 || $scope.mainWifiSSID.length > 31 || /[\':;*?~`!@#$%^&+={}\[\]\<\\(\),\.\。\，]/.test($scope.mainWifiSSID)|| escape($scope.mainWifiSSID).indexOf("%20")!=-1) {
                $scope.wifiPwdIf = true;
                $scope.wifiPwdInfo = '无线名称长度不得超过31位字符或者包含特殊字符！';
                return;
            }
        }
        if (angular.isDefined($scope.mainWifiPasswd)) {
            if ($scope.mainWifiPasswd.length < 8 || $scope.mainWifiPasswd.length > 31 || /[\'\"{}\[\]]/.test($scope.mainWifiPasswd) || escape($scope.mainWifiPasswd).indexOf("%20")!=-1) {
                if($scope.guestWifiPasswd.length!=0){
                    $scope.wifiPwdIf = true;
                    $scope.wifiPwdInfo = '密码长度不能超过31位或不能少于8位！';
                    return;
                }
            }
        }

        var mainWifi = {};
        mainWifi.ssid = $scope.mainWifiSSID;
        mainWifi.key = $scope.mainWifiPasswd;
        mainWifi.hidden = $scope.hideMainWifi;
        for (var i = 0; i < $scope.HTmodels.length; i++) {
            if ($scope.HTmodel == $scope.HTmodels[i]) {
                mainWifi.Channel = i;
                break;
            }
        }
        var data = mainWifi.ssid + '&passwd=' + mainWifi.key + '&hidden=' + mainWifi.hidden + '&bw=' + mainWifi.Channel;
        $http.post(actionUrl + 'fname=net&opt=wifi_ap&function=set&ssid=' + data);
        CheckWebActive($http, null);
    }

    $scope.toggleSubmitVWifi = function () {

        if (angular.isDefined($scope.guestWifiSSID)) {
            if ($scope.guestWifiSSID.length < 1 || $scope.guestWifiSSID.length > 31 || /[\':;*?~`!@#$%^&+={}\[\]\<\\(\),\.\。\，]/.test($scope.guestWifiSSID) || escape($scope.guestWifiSSID).indexOf("%20")!=-1) {
                $scope.wifiGuePwdIf = true;
                $scope.wifiGuePwdInfo = '无线名称长度不得超过31位字符或者包含特殊字符！';
                return;
            }
        }
        if (angular.isDefined($scope.guestWifiPasswd)) {
            if ($scope.guestWifiPasswd.length < 8 || $scope.guestWifiPasswd.length > 31 || /[\'\"{}\[\]]/.test($scope.guestWifiPasswd) || escape($scope.guestWifiPasswd).indexOf("%20")!=-1) {
                if($scope.guestWifiPasswd.length!=0){
                    $scope.wifiGuePwdIf = true;
                    $scope.wifiGuePwdInfo = '密码长度不能超过31位或不能少于8位或者包含特殊字符！';
                    return;
                }
            }
        }
        
        if ($scope.guestWifiSwitch == undefined) {
            $scope.guestWifiSwitch = 0;
        }
        if ($scope.guestWifiHide == undefined) {
            $scope.guestWifiHide = 0;
        }

        var gdata = $scope.guestWifiSSID + '&password=' + $scope.guestWifiPasswd + '&hidden=' + $scope.guestWifiHide + '&enable=' + $scope.guestWifiSwitch;
        $http.post(actionUrl + 'fname=net&opt=wifi_vap&function=set&vssid=' + gdata);
        CheckWebActive($http, null);
    }


    /*wifi密码长度检测不能小于8位*///密码可以为空
    $scope.checkWifiPwd = function (index) {
        switch (index) {
            case 0:
                if ($scope.mainWifiSSID.length < 1 || $scope.mainWifiSSID.length > 31 || /[\':;*?~`!@#$%^&+={}\[\]\<\\(\),\.\。\，]/.test($scope.mainWifiSSID)|| escape($scope.mainWifiSSID).indexOf("%20")!=-1) {
                    $scope.wifiPwdIf = true;
                    $scope.wifiPwdInfo = '无线名称长度不得超过31位字符或者包含特殊字符！';
                }else if ($scope.mainWifiPasswd.length < 8 || $scope.mainWifiPasswd.length > 31|| /[\'\"{}\[\]]/.test($scope.mainWifiPasswd) || escape($scope.mainWifiPasswd).indexOf("%20")!=-1) {
                    if($scope.mainWifiPasswd.length!=0){
                        $scope.wifiPwdIf = true;
                        $scope.wifiPwdInfo = '密码长度不能超过31位或不能少于8位或者包含特殊字符！';
                    }
                }else{
                    $scope.wifiPwdIf = false;
                }
                break;
            case 1:
                
                if ($scope.guestWifiSSID.length < 1 || $scope.guestWifiSSID.length > 31|| /[\':;*?~`!@#$%^&+={}\[\]\<\\(\),\.\。\，]/.test($scope.guestWifiSSID)|| escape($scope.guestWifiSSID).indexOf("%20")!=-1) {
                    $scope.wifiGuePwdIf = true;
                    $scope.wifiGuePwdInfo = '无线名称长度不得超过31位字符或者包含特殊字符！';
                }else if ($scope.guestWifiPasswd.length < 8 || $scope.guestWifiPasswd.length > 31|| /[\'\"{}\[\]]/.test($scope.guestWifiPasswd) || escape($scope.guestWifiPasswd).indexOf("%20")!=-1) {
                    if($scope.guestWifiPasswd.length!=0){
                        $scope.wifiGuePwdIf = true;
                        $scope.wifiGuePwdInfo = '密码长度不能超过31位或不能少于8位或者包含特殊字符！';
                    }
                    
                }else{
                    $scope.wifiGuePwdIf = false;
                }
                break;
            default :
                alert('参数传入错误，请联系开发人员！');
                break;
        }
    }
});
