--
--  \brief  Ada exceptions
--  \author Johannes Kliemann
--  \date   2018-06-25
--
--  Copyright (C) 2018 Genode Labs GmbH
--  Copyright (C) 2018 Componolit GmbH
--
--  This file is part of the Genode OS framework, which is distributed
--  under the terms of the GNU Affero General Public License version 3.
--

with System;

package Ada.Exceptions is

   type Exception_Id is private;
   type Exception_Occurrence is limited private;
   type Exception_Occurrence_Access is access all Exception_Occurrence;

   procedure Raise_Exception_Always (
                                     E : Exception_Id;
                                     Message : String := ""
                                    )
     with
       Export,
       Convention => Ada,
       External_Name => "__gnat_raise_exception";

   procedure Raise_Exception (
                              E : Exception_Id;
                              Message : String := ""
                             );

   procedure Reraise_Occurrence_No_Defer (
                                          X : Exception_Occurrence
                                         );

   procedure Save_Occurrence (
                              Target : out Exception_Occurrence;
                              Source : Exception_Occurrence
                             );

private

   --  the following declarations belong to s-stalib.ads
   --  begin s-stalib.ads
   type Exception_Data;
   type Exception_Data_Ptr is access all Exception_Data;
   type Raise_Action is access procedure;

   type Exception_Data is record
      Not_Handled_By_Others : Boolean;
      Lang                  : Character;
      Name_Length           : Natural;
      Full_Name             : System.Address;
      HTable_Ptr            : Exception_Data_Ptr;
      Foreign_Data          : System.Address;
      Raise_Hook            : Raise_Action;
   end record;
   --  end s-stalib.ads

   type Exception_Id is new Exception_Data_Ptr;
   type Exception_Occurrence is record
      null;
   end record;

   procedure Warn_Not_Implemented (
                                   Name : String
                                  );

end Ada.Exceptions;
