// JavaScript Document
"use strict";
/*
 通用函数调用
 */

//获取url中的参数
function getUrlParam(name) {
    var reg = new RegExp("(^|&)" + name + "=([^&]*)(&|$)"); //构造一个含有目标参数的正则表达式对象
    var r = window.location.search.substr(1).match(reg);  //匹配目标参数
    if (r != null)
        return unescape(r[2]);
    return null; //返回参数值
}
var RET_SUCCESS = 0;
var RET_OVERRIDE = 1;//数据已经存在
var RET_INVALID_LOGIN = 2;//用户登陆错误
var RET_PWD_NOT_EXIST = 3;//缺少密码文件
var RET_FAIL = -1;
var RET_ERR_MSG = -100;
var RET_DB_ACCESSDENIED = 5;  //数据库记录重复，并且owner的用户不是你
//跳转重新登陆错误页面
function isLogin(ret) {

    if (ret == RET_INVALID_LOGIN) {
        window.location = 'index.html';
        return false;
    }
    return true;
}

function getErrorCode(type) {
    switch (type) {
        case 19 :
            return "宽带账号/密码错误！";
            break;
        case 20 :
            return "运营商拨号无响应！";
            break;
        case 21 :
            return "正在获取IP...";
            break;
        case 22 :
            return "已获取IP...";
            break;
        case 10001:
            return "获取参数失败！";
            break;
        case 10002:
            return "输入参数错误！";
            break;
        case 10003:
            return "路由内存不足！";
            break;
        case 10004:
            return "添加的条目已存在！";
            break;
        case 10005:
            return "删除的条目不存在！";
            break;
        case 10006:
            return"添加条目已满！";
            break;
        case 10007:
            return "没登陆！";
            break;
        case 10008:
            return "不支持！";
            break;
        case 10009:
            return"没获取到账号！";
            break;
        case 10010:
            return "时间过期！";
            break;
        case 20100000:
            return "固件暂不支持！";
            break;
        default:
            return "请求超时，错误码：" + type;
    }
}

function errorMessage(code) {
    switch (code) {
        case 0:
            return "连接超时";
            break;
        case 19 :
            return "宽带账号/密码错误！";
            break;
        case 20:
            return "运营商无响应";
            break;
        case 21:
            return "获取IP失败！";
            break;
        case 22:
            return "IP获取成功，无法上网";
            break;
        default:
            return "连接超时";
    }
}


//判断服务器返回结果
function isSuccess(ret) {
    if (angular.isUndefined(ret.error)) {
        return RET_FAIL;
    }
    return RET_SUCCESS;
}
/*显示设置对话框*/
function showSetDialog(name) {
    $("." + name).show();
    $(".routerHidden").fadeIn(400);
}
/*关闭设置对话框*/
function closeSetDialog() {
    $(".routerHidden").fadeOut(0, function () {
        $(".routerSetWifi,.routerSetInter,.routerSetLan,.routerSetPwd,.routerReboot,.routerReset,.routerSetMac,.fileUpModel").hide();
        $(".ycNet").removeAttr('checked');
    });
}

//设置token
function SetToken(token) {
    $('#token').val('token=' + token);
}

//获取token
function GetToken() {
    // return sessionStorage.getItem('token');
    return getUrlParam('token');
}

/*get接口 post token*/
function postToken() {
    var postoken = {
        "token": GetToken()
    }
    return postoken;
}

function ClearStorageCache() {
    sessionStorage.clear();
    localStorage.clear();
}

function IsValidIp(ip) {
    var checkReg = /^\b(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\b$/;
    if (checkReg.test(ip)) {
        return true;
    }
    return false;
}
function IsValidMac(mac) {
    if ((mac.toLowerCase() == 'ff:ff:ff:ff:ff:ff') || (mac.toLowerCase() == '00:00:00:00:00:00')) {
        //   alert('error');
        return false;
    }
    var checkMac = /^([0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}$/;
    if (checkMac.test(mac)) {
        return true;
    }
    return false;
}
/*显示等待窗口*/
function showLoading(msg) {
    $('.loading').show();
    if (msg) {
        $('#loadingTip').text(msg);
    } else {
        $('#loadingTip').text('正在保存设置 请稍候……');
    }

}
/*关闭等待窗口*/
function closeLoading() {
    $('.loading').hide();
}

function tipModel(tipModel, links) {
    closeLoading();
    showSetDialog('tipModel');
    $(".routerSetWifi,.routerSetInter,.routerSetLan,.routerSetPwd,.routerReboot,.routerReset,.routerSetMac").hide();
    $('#tipModelId').text(tipModel);
    $("#submitTip").click(function () {
        closeSetDialog();
        $('.tipModel').hide();
        if (links) {
            window.location.href = links;
        }

    });
}

/*url跳转*/
function urlHref($scope) {
    $scope.stateHref = 'state.html';
    $scope.deviceHref = 'device.html';
    $scope.settingHref = 'setting.html';
}




/*
终端黑白名单控制 2016/08/30
 */

function blackMac(type){

    var list=getBlacklist(),acl_type="white",len=list.length;
    if(!type){
        acl_type="black";
    }
    if(list.length<0){
        list="empty";
    }
    $.post(actionUrl + 'fname=net&opt=acl_'+acl_type+'&function=set&ifindex=0&mac='+list.join(";")+'&mac_num='+len+'&enable=0', function (data) {
        if (data.error == 0) {
            set(type);
        }
    },"json")

    function set(_acl_type){
        $.post(actionUrl + 'fname=net&opt=acl_'+Fdata(_acl_type)+'&function=set&ifindex=0&mac=empty&mac_num=0&enable=1', function (data) {
            if (data.error == 0) {
                var _msg=_acl_type?"允许":"禁止";
                tipModel("设置成功！您已"+_msg+"以下MAC地址访问！");
            }
        },"json")
    
    }

}

function getBlacklist(){
    var macList=[],macItem=$("#macUl").find("li")
    for(var i=0,len=macItem.length;i<len;i++){
        var macName=macItem.eq(i).find("input:text").val()
        if(macName != "" && macName != null){
            macList.push(macName);
        }
    }
    return macList;
}

function Fdata(type){
    return type?"black":"white";
}

