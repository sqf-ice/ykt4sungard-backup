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

package com.liferay.portlet.mail.action;

import com.liferay.portal.struts.PortletAction;
import com.liferay.portal.util.Constants;
import com.liferay.portlet.mail.util.FolderPool;
import com.liferay.portlet.mail.util.MailUtil;
import com.liferay.util.ListUtil;
import com.liferay.util.ParamUtil;
import com.liferay.util.StringComparator;
import com.liferay.util.Validator;
import com.liferay.util.servlet.SessionErrors;
import com.liferay.util.servlet.SessionMessages;

import java.util.Collections;
import java.util.List;

import javax.portlet.ActionRequest;
import javax.portlet.ActionResponse;
import javax.portlet.PortletConfig;
import javax.portlet.PortletPreferences;

import javax.servlet.jsp.PageContext;

import org.apache.struts.action.ActionForm;
import org.apache.struts.action.ActionMapping;

/**
 * <a href="AddFolderAction.java.html"><b><i>View Source</i></b></a>
 *
 * @author  Brian Wing Shun Chan
 * @version $Revision: 1.4 $
 *
 */
public class AddFolderAction extends PortletAction {

	public void processAction(
			ActionMapping mapping, ActionForm form, PortletConfig config,
			ActionRequest req, ActionResponse res)
		throws Exception {

		String cmd = req.getParameter(Constants.CMD);

		if (cmd == null) {
			setForward(req, "portlet.mail.add_folder");
		}
		else {
			try {
				_validateFolder(req);

				if (!SessionErrors.isEmpty(req)) {
					setForward(req, "portlet.mail.add_folder");
				}
				else {
					_addFolder(req, res);
				}
			}
			catch (Exception e) {
				req.setAttribute(PageContext.EXCEPTION, e);

				setForward(req, Constants.COMMON_ERROR);
			}
		}
	}

	private void _addFolder(ActionRequest req, ActionResponse res)
		throws Exception {

		PortletPreferences prefs = req.getPreferences();

		String folderName = ParamUtil.getString(req, "folder_name");

		if (Validator.isNotNull(folderName)) {
			List list = ListUtil.fromArray(
				prefs.getValues("folder-names", new String[0]));

			list.add(folderName);

			Collections.sort(list, new StringComparator());

			prefs.setValues(
				"folder-names", (String[])list.toArray(new String[0]));
		}

		prefs.store();

		// Session messages

		SessionMessages.add(req, AddFolderAction.class.getName());

		// Send redirect

		res.sendRedirect(ParamUtil.getString(req, "redirect"));
	}

	private void _validateFolder(ActionRequest req) throws Exception {
		String folderName = ParamUtil.getString(req, "folder_name");

		if (!MailUtil.isValidFolderName(folderName)) {
			SessionErrors.add(req, "folder_name_invalid");
		}
		else {
			if (!folderName.equals(MailUtil.MAIL_INBOX_NAME)) {
				if (!folderName.startsWith(MailUtil.MAIL_BOX_STYLE)) {
					folderName = MailUtil.MAIL_BOX_STYLE + folderName;
				}
			}

			FolderPool folderPool = MailUtil.getFolderPool(req);

			if (folderPool.get(folderName) != null) {
				SessionErrors.add(req, "folder_name_already_taken");
			}
		}
	}

}