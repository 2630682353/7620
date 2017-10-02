/**
 * Created by lad on 2016/6/1.
 * 重置密码控制器
 */

app.controller('passwdCtrl', function ($scope, $http) {
    $scope.newPasswd = $scope.newPassRepeat = '';
    $scope.submitPasswd = function () {
        //console.log('submitPasswd');
        if ($scope.newPasswd != $scope.newPassRepeat) {
            $scope.newPassRepeatErr = '密码不一致，请重新输入！';
            return false;
        }
        $scope.errMsg = '';
        if ($scope.checkSame() === false) {
            return false;
        }
        showLoading(null);
        $http.post(actionUrl + 'fname=system&opt=login_account&function=set&user=admin&password=' + $scope.newPasswd)
                .error(function () {
                    showRet(null);
                })
                .success(function (ret) {
                    if (ret.error == 0) {
                        closeLoading();
                        closeSetDialog();
                        ClearStorageCache();
                        // alert('设置成功！');
                        var tipText = '设置成功！';
                        tipModel(tipText, 'login.html');
                        // gotoDelay('login.html', 0);
                    } else {
                        $scope.errMsg = ret.error;
                        alert($scope.errMsg);
                    }
                });
    }
    /*检查密码规则*/
    $scope.checkNewPass = function () {

    }
    /*检查密码的一致性*/
    $scope.checkSame = function () {
        //console.log('checkSame');
        if ($scope.newPasswd != $scope.newPassRepeat) {
            $scope.newPassRepeatErr = '密码不一致，请重新输入！';
            return false;
        }
        if ($scope.newPasswd.length > 15 || $scope.newPasswd.length < 5) {
            $scope.newPassRepeatErr = '密码长度不能超过15位或者小于5位！';
            return false;
        }

        if (escape($scope.newPasswd).indexOf("%u") != -1 || /[\':;*?~`!@#$%^&+={}\[\]\<\\(\),\.\。\，]/.test($scope.newPasswd)) {
            $scope.newPassRepeatErr = '密码不能包含中文字符或者特殊字符！';
            return false;
        }

    }

    $scope.clearPasswdErr = function () {
        $scope.newPassErr = '';
        $scope.newPassRepeatErr = '';
    }
});