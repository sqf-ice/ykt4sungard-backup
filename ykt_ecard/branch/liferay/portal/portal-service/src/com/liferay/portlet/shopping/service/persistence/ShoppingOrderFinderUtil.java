/**
 * Copyright (c) 2000-2008 Liferay, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

package com.liferay.portlet.shopping.service.persistence;

/**
 * <a href="ShoppingOrderFinderUtil.java.html"><b><i>View Source</i></b></a>
 *
 * @author Brian Wing Shun Chan
 *
 */
public class ShoppingOrderFinderUtil {
	public static int countByG_C_U_N_PPPS(long groupId, long companyId,
		long userId, java.lang.String number,
		java.lang.String billingFirstName, java.lang.String billingLastName,
		java.lang.String billingEmailAddress,
		java.lang.String shippingFirstName, java.lang.String shippingLastName,
		java.lang.String shippingEmailAddress,
		java.lang.String ppPaymentStatus, boolean andOperator)
		throws com.liferay.portal.SystemException {
		return getFinder().countByG_C_U_N_PPPS(groupId, companyId, userId,
			number, billingFirstName, billingLastName, billingEmailAddress,
			shippingFirstName, shippingLastName, shippingEmailAddress,
			ppPaymentStatus, andOperator);
	}

	public static java.util.List findByG_C_U_N_PPPS(long groupId,
		long companyId, long userId, java.lang.String number,
		java.lang.String billingFirstName, java.lang.String billingLastName,
		java.lang.String billingEmailAddress,
		java.lang.String shippingFirstName, java.lang.String shippingLastName,
		java.lang.String shippingEmailAddress,
		java.lang.String ppPaymentStatus, boolean andOperator, int begin,
		int end, com.liferay.portal.kernel.util.OrderByComparator obc)
		throws com.liferay.portal.SystemException {
		return getFinder().findByG_C_U_N_PPPS(groupId, companyId, userId,
			number, billingFirstName, billingLastName, billingEmailAddress,
			shippingFirstName, shippingLastName, shippingEmailAddress,
			ppPaymentStatus, andOperator, begin, end, obc);
	}

	public static ShoppingOrderFinder getFinder() {
		return _getUtil()._finder;
	}

	public void setFinder(ShoppingOrderFinder finder) {
		_finder = finder;
	}

	private static ShoppingOrderFinderUtil _getUtil() {
		if (_util == null) {
			_util = (ShoppingOrderFinderUtil)com.liferay.portal.kernel.bean.BeanLocatorUtil.locate(_UTIL);
		}

		return _util;
	}

	private static final String _UTIL = ShoppingOrderFinderUtil.class.getName();
	private static ShoppingOrderFinderUtil _util;
	private ShoppingOrderFinder _finder;
}