
#include "holiday.h"
#include "calendar.h"


enum DayOfWeek {
	Sunday,
	Monday,
	Tuesday,
	Wednesday,
	Thursday,
	Friday,
	Saturday,
};

// 国民の祝日
// http://ja.wikipedia.org/wiki/%E5%9B%BD%E6%B0%91%E3%81%AE%E7%A5%9D%E6%97%A5
char const *KokuminNoShukujitu(int year, int month, int day)
{
	int jdn = ToJDN(year, month, day);				// JDN
	int weeknum = (day + 6) / 7;						// 第何
	DayOfWeek dayweek = (DayOfWeek)((jdn + 1) % 7);		// 曜日

	// 元日 - 1月1日（1949年より）

	if (month == 1 && day == 1) return "元日";

	// 成人の日 - 1月の第2月曜日（2000年より。1949〜1999年は1月15日）

	if (year >= 1949 && year <= 1999 && month == 1 && day == 15) return "成人の日";
	if (year >= 2000 && month == 1 && weeknum == 2 && dayweek == Monday) return "成人の日";

	// 建国記念の日 - 政令で定める日（＝2月11日）（1967年より）

	if (year >= 1967 && month == 2 && day == 11) return "建国記念の日";

	// 春分の日 - 春分日（1949年より。21世紀前半は3月20日または21日）

	if (year >= 1980 && year <= 2099 && month == 3 && day == (int)(20.8431 + 0.242194 * (year - 1980) - (int)((year - 1980) / 4))) {
		return "春分の日";
	}

	// 昭和の日 - 4月29日（2007年より）

	if (year >= 2007 && month == 4 && day == 29) return "昭和の日";

	// 憲法記念日 - 5月3日（1949年より）

	if (year >= 1949 && month == 5 && day == 3) return "憲法記念日";

	// みどりの日 - 5月4日（1989〜2006年は4月29日）　1988〜2006年の火〜土曜日は「国民の休日」だった

	if (year >= 1989 && year <= 2006 && month == 4 && day == 29) return "みどりの日";
	if (year >= 2007 && month == 5 && day == 4) return "みどりの日";

	// こどもの日 - 5月5日（1949年より）

	if (year >= 1949 && month == 5 && day == 5) return "こどもの日";

	// 海の日 - 7月の第3月曜日（2003年より。1996〜2002年は7月20日）

	if (year >= 1996 && year <= 2002 && month == 7 && day == 20) return "海の日";
	if (year >= 2003 && month == 7 && weeknum == 3 && dayweek == Monday) return "海の日";

	// 敬老の日 - 9月の第3月曜日（2003年より。1966〜2002年は9月15日）

	if (year >= 1966 && year <= 2002 && month == 9 && day == 15) return "敬老の日";
	if (year >= 2003 && month == 9 && weeknum == 3 && dayweek == Monday) return "敬老の日";

	// 秋分の日 - 秋分日（1948年より。21世紀前半は9月22日または23日）

	if (year >= 1980 && year <= 2099 && month == 9 && day == (int)(23.2488 + 0.242194 * (year - 1980) - (int)((year - 1980) / 4))) {
		return "秋分の日";
	}

	// 体育の日 - 10月の第2月曜日（2000年より。1966〜1999年は10月10日）

	if (year >= 1966 && year <= 1999 && month == 10 && day == 10) return "体育の日";
	if (year >= 2000 && month == 10 && weeknum == 2 && dayweek == Monday) return "体育の日";

	// 文化の日 - 11月3日（1948年より）

	if (year >= 1948 && month == 11 && day == 3) return "文化の日";

	// 勤労感謝の日 - 11月23日（1948年より）

	if (year >= 1948 && month == 11 && day == 23) return "勤労感謝の日";

	// 天皇誕生日 - 12月23日（1989年より。1949〜1988年は4月29日）

	if (year >= 1949 && year <= 1988 && month == 4 && day == 29) return "天皇誕生日";
	if (year >= 1989 && month == 12 && day == 23) return "天皇誕生日";

	// 振替休日

	if (dayweek == Monday) {
		if (KokuminNoShukujitu(year, month, day - 1) != 0) {
			return "振替休日";
		}
	}

	// 振り替えの振り替え
	if (year >= 2007 && month == 5 && day == 6) {
		if (dayweek == Monday || dayweek == Tuesday || dayweek == Wednesday) {
			return "振替休日";
		}
	}

	// その他
	// http://ja.wikipedia.org/wiki/%E5%9B%BD%E6%B0%91%E3%81%AE%E4%BC%91%E6%97%A5

	if (year >= 1986 && year <= 2006 && month == 5 && day == 4) return "国民の休日";
	if ((year == 2009 || year == 2015) && month == 9 && day == 22) return "国民の休日";

	// 平日

	return 0;
}
