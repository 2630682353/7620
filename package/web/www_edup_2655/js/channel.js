/* 
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
var channel = [], channel2 = [],setState=0;

channel[0] = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11];
channel[1] = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13];
channel[2] = [10, 11];
channel[3] = [10, 11, 12, 13];
channel[4] = [14];
channel[5] = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14];
channel[6] = [3, 4, 5, 6, 7, 8, 9];
channel[7] = [5, 6, 7, 8, 9, 10, 11, 12, 13];

channel[31] = channel[5];
channel[32] = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11];
//channel[33] = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14];
channel[33] = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13];


channel2[0] = [36, 40, 44, 48, 52, 56, 60, 64, 149, 153, 157, 161, 165];
channel2[1] = [36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140];
channel2[2] = [36, 40, 44, 48, 52, 56, 60, 64];
channel2[3] = [52, 56, 60, 64, 149, 153, 157, 161];
channel2[4] = [149, 153, 157, 161, 165];
channel2[5] = [149, 153, 157, 161];
channel2[6] = [36, 40, 44, 48];
channel2[7] = [36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 149, 153, 157, 161, 165];
channel2[8] = [52, 56, 60, 64];
channel2[9] = [36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 132, 136, 140, 149, 153, 157, 161, 165];
channel2[10] = [36, 40, 44, 48, 149, 153, 157, 161, 165];
channel2[11] = [36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 149, 153, 157, 161, 165];

/*
		global == 1,1

	   fcc(us) == 0,10
   
   ce/etsi(eu) == 1,6
	
	 japan(jp) == 33,6
	
	france(fr) == 1,2
	
	 china(cn) == 1,4
  
  hongkong(hk) == 1,0
*/


//区域-信道 二级联动

$(function(){
	$("#country").change(function(){
		var key=$(this).val();
		var wifiChannel=$("#wifiChannel");
		var optionlist="<option value='0'>Auto</option>";
		$.each(channel[key],function(i,value_){
			optionlist+='<option value='+value_+'>'+value_+'</option>';
		})		
		wifiChannel.empty().html(optionlist);
		setdefaultWifiAp(key);
	})

})


