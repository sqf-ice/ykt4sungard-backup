/* --------------------------------------------
 * 程序名称: F848256.c
 * 创建日期: 2007-04-03
 * 程序作者: 汤成
 * 版本信息: 1.0.0.0
 * 程序功能:门禁节假日管理
 * --------------------------------------------
 * 修改日期:
 * 修改人员:
 * 修改描述:
 * 版本信息:
 * 备注信息:
 * --------------------------------------------*/
#define _IN_SQC_
#include <string.h>
#include <stdio.h>
#include "pubfunc.h"
#include "pubdb.h"
#include "pubdef.h"
#include "errdef.h"
#include "dbfunc.h"
#include "fdsqc.h"
#include "dbfunc_foo.h"

static int do_848256(TRUSERID *handle,int iRequest,ST_PACK *in_pack,int *pRetCode,char *szMsg)
{
	T_t_door_holiday holiday;
	T_t_door_holiday_times times;
	T_t_door_times_group tgroup;
	int ret;

	memset(&holiday,0,sizeof holiday);
	memset(&times,0,sizeof times);
	memset(&tgroup,0,sizeof tgroup);

	if(unlike_tbl_id(in_pack->lvol0))
		return E_INPUT_ERROR;

	ret = IsInvalidDateTime(in_pack->sdate0,"YYYYMMDD");
	if(ret)
	{
		sprintf(szMsg,"日期格式错误[%s]",in_pack->sdate0);
		writelog(LOG_ERR,szMsg);
		return ret;
	}

	if(unlike_tbl_id(in_pack->lvol1))
	{
		sprintf(szMsg,"日期[%s] 需选择时间段组",in_pack->sdate0);
		writelog(LOG_ERR,szMsg);
		return E_INPUT_ERROR;
	}
	
	ret = DB_t_door_holiday_read_by_holiday_id(in_pack->lvol0,&holiday);
	if(ret)
	{
		if(DB_NOTFOUND == ret)
		{
			return E_DB_DOOR_HOLIDAY_N;
		}
		return E_DB_DOOR_HOLIDAY_R;
	}

	ret = DB_t_door_times_group_read_by_tgid(in_pack->lvol1,&tgroup);
	if(ret)
	{
		if(DB_NOTFOUND == ret)
			return E_DB_DOOR_TIME_GROUP_N;
		return E_DB_DOOR_TIME_GROUP_R;
	}
	
	if(in_pack->sstatus0[0] == '1'
	|| in_pack->sstatus0[0] == '3' )
	{
		ret = DB_t_door_holiday_times_del_by_hid(in_pack->lvol0);
		if(ret)
		{
			if(ret != DB_NOTFOUND)
				return E_DB_DOOR_HOLIDAY_TIME_D;
		}
	}
	times.hid = in_pack->lvol0;
	des2src(times.h_date,in_pack->sdate0);
	times.time_grp_id = in_pack->lvol1;
	times.flag = DOOR_FLAG_NORMAL;
	getsysdatetime(times.last_update);

	ret = DB_t_door_holiday_times_add(&times);
	if(ret)
	{
		if(DB_REPEAT == ret)
		{
			// 更新
			ret = DB_t_door_holiday_times_update_by_hid_and_h_date(
			in_pack->lvol0,in_pack->sdate0,&times);
			if(ret)
			{
				writelog(LOG_ERR,"更新节假日设置信息失败,hid[%d] date[%s]"
				,in_pack->lvol0,in_pack->sdate0);
				sprintf(szMsg,"更新节假日[%s] 失败",in_pack->sdate0);
				return E_DB_DOOR_HOLIDAY_TIME_U;
			}
		}
		else
		{
			sprintf(szMsg,"增加时间段[%s] 失败",in_pack->sdate0);
			writelog(LOG_ERR,szMsg);
			return E_DB_DOOR_HOLIDAY_TIME_I;
		}
	}

	if(in_pack->sstatus0[0] == '2'
	||in_pack->sstatus0[0] == '3' )
	{
		// 更新时间段组表
		des2src(holiday.last_update,times.last_update);
		ret = count_times_of_holiday(holiday.holiday_id,&(holiday.day_cnt));
		if(ret)
		{
			sprintf(szMsg,"统计节假日[%s] 时段错误",holiday.holiday_name);
			writelog(LOG_ERR,szMsg);
			return ret;
		}
		ret = DB_t_door_holiday_update_by_holiday_id(holiday.holiday_id,&holiday);
		if(ret)
		{
			
			sprintf(szMsg,"完成节假日设置,更新时间截失败,[%s]",holiday.holiday_name);
			writelog(LOG_ERR,szMsg);
			if(DB_NOTFOUND == ret)
				return E_DB_DOOR_HOLIDAY_N;
			return E_DB_DOOR_HOLIDAY_U;
		}
	}
	return 0;
}

int F848256(TRUSERID *handle,int iRequest,ST_PACK *in_pack,int *pRetCode,char *szMsg)
{
	int ret;
	ret = do_848256(handle,iRequest,in_pack,pRetCode,szMsg);
	if(ret)
	{
		*pRetCode = ret;
		return -1;
	}
	return 0;
}

