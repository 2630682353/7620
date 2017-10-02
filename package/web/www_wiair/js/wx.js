var deviceid = '';
var mac = getQueryString('mac');
var vssid = '';
var actionUrl = "http://" + document.domain + "/protocol.csp?";
$(function () {
    wechat_allow(mac);
    $("#wechatUrl").click(function () {
        wifi_vap();
        DeviceId();
        var wechatUrl = 'weixin://connectToFreeWifi/friendWifi?appid=wxf52d70b98ad2b6cd&devicetype=gh_394be17b8339&deviceid=' + deviceid + '&clientinfo=' + mac + '&ssid=' + vssid;
        $("#wechatUrl").attr('href', wechatUrl);
    });
});



function DeviceId()
{
    $.ajax({
        url: actionUrl + 'fname=system&opt=deviceid&function=get&math=' + Math.random(),
        type: "POST",
        dataType: 'JSON',
        async: false,
        success: function (data) {
            if (data.error == 0) {
                deviceid = data.deviceid;
            }
        }, error: function (XHRequest, status, data) {
            XHRequest.abort();
        }
    });
}

function wifi_vap()
{
    $.ajax({
        url: actionUrl + 'fname=net&opt=wifi_vap&function=get&math=' + Math.random(),
        type: "POST",
        dataType: 'JSON',
        async: false,
        success: function (data) {
            if (data.error == 0) {
                vssid = data.vssid;
            }
        }, error: function (XHRequest, status, data) {
            XHRequest.abort();
        }
    });
}

function wechat_allow(mac) {
    $.ajax({
        url: actionUrl + 'fname=net&opt=wechat_allow&function=set&mac=' + mac + '&math=' + Math.random(),
        type: "POST",
        dataType: 'JSON',
        async: false,
        success: function (data) {
            if (data.error == 0) {

            }
        }, error: function (XHRequest, status, data) {
            XHRequest.abort();
        }
    });
}

function getQueryString(name) {
    var reg = new RegExp("(^|&)" + name + "=([^&]*)(&|$)");
    var r = window.location.search.substr(1).match(reg);
    if (r != null) {
        return unescape(r[2]);
    }
    return null;
}