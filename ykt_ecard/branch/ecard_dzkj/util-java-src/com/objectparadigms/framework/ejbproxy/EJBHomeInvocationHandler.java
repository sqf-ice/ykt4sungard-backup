/**
 * Copyright (c) 2000-2005 Liferay, LLC. All rights reserved.
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

package com.objectparadigms.framework.ejbproxy;

import com.objectparadigms.framework.ejbproxy.config.EJBProxyConfig;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;

/**
 * <a href="EJBHomeInvocationHandler.java.html"><b><i>View Source</i></b></a>
 *
 * @author  Michael C. Han
 * @version $Revision: 1.4 $
 *
 */
public class EJBHomeInvocationHandler extends EJBInvocationHandler {
	private static final String _EJB_CREATE_METHOD = "create";

	public Object invoke(Object proxy, Method method, Object[] args)
		throws Throwable {
		Object obj = null;

		try {
			obj = method.invoke(getDelegate(), args);
		}
		catch (InvocationTargetException e) {
			throw e.getTargetException();
		}

		if (!method.getName().equals(_EJB_CREATE_METHOD)) {
			return obj;
		}

		EJBProxyConfig ejbConfig = getConfig();
		EJBInvocationHandler handler = (EJBInvocationHandler)ejbConfig.getRemoteHandler()
																	  .newInstance();
		handler.setDelegate(obj);
		handler.setConfig(ejbConfig);

		return Proxy.newProxyInstance(obj.getClass().getClassLoader(),
			new Class[] { ejbConfig.getRemoteClass() }, handler);
	}
}