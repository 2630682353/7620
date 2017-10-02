/**
 * Created by lad on 2016/6/5.
 * 增删白名单和黑名单控制接口
 */

var g_postJson = {
    'content' : {
        'macfilter' : new Array(),//黑名单列表
        'enable' : 1,//白名单的全局控制开关
        'mactrusted' : new Array(),//白名单列表

    },
};
/*
*   查找白名单列表中是否包含有指定的mac
*   @param mac:需要查找的mac
*   return int：在列表中的位置，
*   如果不存在，则返回-1
*
* */
function ContainInTrusted(mac){
    var trustList = g_postJson.content.mac;
    if(trustList === undefined){
        trustList = g_postJson.content.mactrusted;
    }
    for(var i = 0; i < trustList.length; i++){
        if(mac == trustList[i].mac){
            return i;
        }
    }
    return -1;
}

//function ContainInTrusted2(mac){
//    var trustList = g_postJson.content.mactrusted;
//    for(var i = 0; i < trustList.length; i++){
//        if(mac == trustList[i].mac){
//            return i;
//        }
//    }
//    return -1;
//}

function AddWhiteName(mac, enable){
    var trustList = g_postJson.content.mac;
    if(trustList === undefined){
        trustList = g_postJson.content.mactrusted;
    }    
    var index = ContainInTrusted(mac);
    var item = {
        'mac' : mac,
        'enable' : enable,
    };
    if(-1 == index){
        trustList.push(item);
    }else{
        var item = trustList[index];
        item.enable = enable;
    }
}

//function AddWhiteName2(mac, enable){
//    var trustList = g_postJson.content.mactrusted;
//    var index = ContainInTrusted(mac);
//    var item = {
//        'mac' : mac,
//        'enable' : enable,
//    };
//    if(-1 == index){
//        trustList.push(item);
//    }else{
//        var item = trustList[index];
//        item.enable = enable;
//    }
//}

function EnableFireWall(enable){
    g_postJson.content.enable = enable;
}

function SetFireWallContent(content){
    g_postJson.content = content;
}
function GetFireWallEnable(){
    return g_postJson.content.enable;
}

