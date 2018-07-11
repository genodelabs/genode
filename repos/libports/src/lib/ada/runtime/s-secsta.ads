--
--  \brief  Ada secondary stack
--  \author Johannes Kliemann
--  \date   2018-04-16
--
--  Copyright (C) 2018 Genode Labs GmbH
--  Copyright (C) 2018 Componolit GmbH
--
--  This file is part of the Genode OS framework, which is distributed
--  under the terms of the GNU Affero General Public License version 3.
--

with System.Storage_Elements;
with Ss_Utils;
use all type Ss_Utils.Thread;

package System.Secondary_Stack is

   package SSE renames System.Storage_Elements;

   type Mark_Id is private;

   procedure SS_Allocate (
                          Address      : out System.Address;
                          Storage_Size : SSE.Storage_Count
                         );

   function SS_Mark return Mark_Id;

   procedure SS_Release (
                         M : Mark_Id
                        );

private

   SS_Pool : Integer;
   --  This is not used but needed since the build will fail otherwise

   type Mark_Id is record
      Sstk : System.Address;
      Sptr : SSE.Integer_Address;
   end record;

   Thread_Registry : Ss_Utils.Registry := Ss_Utils.Null_Registry;

end System.Secondary_Stack;
