/*
 * //******************************************************************
 * //
 * // Copyright 2016 Samsung Electronics All Rights Reserved.
 * //
 * //-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
 * //
 * // Licensed under the Apache License, Version 2.0 (the "License");
 * // you may not use this file except in compliance with the License.
 * // You may obtain a copy of the License at
 * //
 * //      http://www.apache.org/licenses/LICENSE-2.0
 * //
 * // Unless required by applicable law or agreed to in writing, software
 * // distributed under the License is distributed on an "AS IS" BASIS,
 * // WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * // See the License for the specific language governing permissions and
 * // limitations under the License.
 * //
 * //-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
 */
package org.iotivity.cloud.accountserver.resources.account.session;

import java.util.Arrays;
import java.util.HashMap;

import org.iotivity.cloud.accountserver.Constants;
import org.iotivity.cloud.accountserver.resources.account.AccountManager;
import org.iotivity.cloud.base.device.Device;
import org.iotivity.cloud.base.exception.ServerException;
import org.iotivity.cloud.base.exception.ServerException.BadRequestException;
import org.iotivity.cloud.base.protocols.IRequest;
import org.iotivity.cloud.base.protocols.IResponse;
import org.iotivity.cloud.base.protocols.MessageBuilder;
import org.iotivity.cloud.base.protocols.enums.ContentFormat;
import org.iotivity.cloud.base.protocols.enums.ResponseStatus;
import org.iotivity.cloud.base.resource.Resource;
import org.iotivity.cloud.util.Cbor;

public class SessionResource extends Resource {

    private Cbor<HashMap<String, Object>> mCbor      = new Cbor<>();

    private AccountManager                mAsManager = new AccountManager();

    public SessionResource() {
        super(Arrays.asList(Constants.PREFIX_OIC, Constants.ACCOUNT_URI,
                Constants.SESSION_URI));
    }

    @Override
    public void onDefaultRequestReceived(Device srcDevice, IRequest request)
            throws ServerException {

        IResponse response = null;

        switch (request.getMethod()) {
            case POST:
                // Used for user sign-in & out
                response = handlePostSigninout(request);
                break;

            default:
                throw new BadRequestException(
                        request.getMethod() + " request type is not support");
        }

        srcDevice.sendResponse(response);
    }

    private IResponse handlePostSigninout(IRequest request)
            throws ServerException {

        HashMap<String, Object> payloadData = mCbor
                .parsePayloadFromCbor(request.getPayload(), HashMap.class);

        if (payloadData == null) {
            throw new BadRequestException("payload is null");
        }

        HashMap<String, Object> responsePayload = null;

        if (checkPayloadException(
                Arrays.asList(Constants.REQ_UUID_ID, Constants.REQ_DEVICE_ID,
                        Constants.REQ_ACCESS_TOKEN, Constants.REQ_LOGIN),
                payloadData)) {

            String uuid = payloadData.get(Constants.REQ_UUID_ID).toString();
            String deviceId = payloadData.get(Constants.REQ_DEVICE_ID)
                    .toString();
            String accessToken = payloadData.get(Constants.REQ_ACCESS_TOKEN)
                    .toString();
            boolean signinRequest = (boolean) payloadData
                    .get(Constants.REQ_LOGIN);

            if (signinRequest) {
                responsePayload = mAsManager.signInOut(uuid, deviceId,
                        accessToken);
            } else {
                mAsManager.signInOut(uuid, deviceId, accessToken);
                return MessageBuilder.createResponse(request,
                        ResponseStatus.CHANGED);
            }
        }

        return MessageBuilder.createResponse(request, ResponseStatus.CHANGED,
                ContentFormat.APPLICATION_CBOR,
                mCbor.encodingPayloadToCbor(responsePayload));
    }
}
