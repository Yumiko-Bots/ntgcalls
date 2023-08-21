import asyncio
import logging

from ntgcalls import NTgCalls
from pyrogram import Client, idle
from pyrogram.raw.functions.channels import GetFullChannel
from pyrogram.raw.functions.phone import JoinGroupCall
from pyrogram.raw.types import UpdateGroupCallConnection, Updates, DataJSON, InputChannel

api_id = 2799555
api_hash = '47d66bbf0939d0ddf32caf8bad590ed7'

wrtc = NTgCalls()


async def main():
    client = Client('test', api_id, api_hash, sleep_threshold=1)

    file_audio = "C:/Users/iraci/PycharmProjects/NativeTgCalls/tools/output.pcm"


    async with client:
        call_params = wrtc.createCall(file_audio)
        chat = await client.resolve_peer(-1001398466177)
        local_peer = await client.resolve_peer((await client.get_me()).id)
        input_call = (
            await client.invoke(
                GetFullChannel(
                    channel=InputChannel(
                        channel_id=chat.channel_id,
                        access_hash=chat.access_hash,
                    ),
                ),
            )
        ).full_chat.call
        result: Updates = await client.invoke(
            JoinGroupCall(
                call=input_call,
                params=DataJSON(data=call_params),
                video_stopped=False,
                muted=False,
                join_as=local_peer,
            ), sleep_threshold=0, retries=0
        )
        for update in result.updates:
            if isinstance(update, UpdateGroupCallConnection):
                wrtc.setRemoteCallParams(update.params.data)
        print("Connected!")
        await idle()
        print("Closed")

asyncio.new_event_loop().run_until_complete(main())
